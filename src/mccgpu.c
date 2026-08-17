/* Device layer for the width ladder's GPU oracle.
 *
 * Vulkan + SPIR-V on every host but Darwin, Metal + MSL there.  Both loaders
 * are dlopened at first use, so nothing here is on any link line: a host with
 * no driver leaves mcc_gpu_dispatch() returning 0 and the ladder on its CPU
 * oracle.  The shader emitters that produce `code` live in mccgpu.h; this file
 * never looks at an AST.
 *
 * The Vulkan declarations below are transcribed from the Khronos
 * Vulkan-Headers project -- see src/mccgpu.LICENSE.
 */

#include <fenv.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mccgpu.h"

#ifndef MCC_GPU_MALLOC
#define MCC_GPU_MALLOC malloc
#endif
#ifndef MCC_GPU_FREE
#define MCC_GPU_FREE free
#endif

#if MCC_HOST_POSIX
#include <pthread.h>
static pthread_mutex_t mcc_gpu_lock = PTHREAD_MUTEX_INITIALIZER;
#define MCC_GPU_LOCK() pthread_mutex_lock(&mcc_gpu_lock)
#define MCC_GPU_UNLOCK() pthread_mutex_unlock(&mcc_gpu_lock)
#else
#define MCC_GPU_LOCK() ((void)0)
#define MCC_GPU_UNLOCK() ((void)0)
#endif

/* fegetenv/fesetenv are resolved dynamically for the same reason the drivers
 * are: an --embed-jit program has no business linking libm. */
typedef int (*MccFeGetFn)(fenv_t *);
typedef int (*MccFeSetFn)(const fenv_t *);

static MccFeGetFn mcc_fe_get;
static MccFeSetFn mcc_fe_set;

static int mcc_gpu_closing;

/* Aggregate GPU dispatch counters, shared across the Metal and Vulkan arms and
 * (for Vulkan) across all held devices (T-win-50022). Named `mcc_gpu_ctr` to
 * avoid colliding with the public accessor function mcc_gpu_stranded(). */
static struct {
	long dispatches;
	long lanes;
	long stranded;
} mcc_gpu_ctr;

/* T-lin-10393 runtime budgets, set from --jit-gpu-* in mcc_parse_args (same TU
 * via libmcc.c's #include of this file). -1 = unset. Defined unconditionally --
 * outside the MSL/Vulkan split -- so libmcc.c's unconditional setter resolves on
 * every backend (the MSL build has no Vulkan branch to hold them). */
static int mcc_gpu_vram_budget_pct = -1;
static int mcc_gpu_max_devices = -1;

#if MCC_GPU_LANG_MSL

#include <objc/message.h>
#include <objc/runtime.h>

typedef id (*MccObjcGetClassFn)(const char *);
typedef SEL (*MccObjcSelFn)(const char *);

static MccObjcGetClassFn mcc_objc_getClass;
static MccObjcSelFn mcc_sel_registerName;
static void *mcc_objc_msgSend;

#define objc_getClass mcc_objc_getClass
#define sel_registerName mcc_sel_registerName
#define objc_msgSend mcc_objc_msgSend

#define MCC_MTL_UTF8 4
#define MCC_MTL_CACHE_MAX 64
#define MCC_MTL_CB_COMPLETED 4
#define MCC_MTL_ERR_ENCODER_STATUS 1
#define MCC_MTL_ENC_FAULTED 4
#define MCC_MTL_MIN_BUFFER (64UL * 1024 * 1024)
#define MCC_MTL_MATH_SAFE 0

enum {
	MCC_MTL_FAULT_NONE,
	MCC_MTL_FAULT_WATCHDOG,
	MCC_MTL_FAULT_HANG,
	MCC_MTL_FAULT_PAGEFAULT,
	MCC_MTL_FAULT_INNOCENT,
	MCC_MTL_FAULT_TIMEOUT,
	MCC_MTL_FAULT_OOM,
	MCC_MTL_FAULT_DEVICE_LOST,
	MCC_MTL_FAULT_OVER_LIMIT,
	MCC_MTL_FAULT_ENCODE,
	MCC_MTL_FAULT_UNCLASSIFIED,
	MCC_MTL_FAULT_N
};

static const char *const mcc_mtl_fault_names[MCC_MTL_FAULT_N] = {
		"none",					 "watchdog",			"hang",					"page-fault",
		"innocent-victim", "timeout",				"out-of-memory", "device-lost",
		"over-limit",			 "encode-failed", "unclassified"};

typedef id (*MccMtlCreateDeviceFn)(void);
typedef id (*MccMtlCopyDevicesFn)(void);

static MccMtlCreateDeviceFn mcc_mtl_create_device;
static MccMtlCopyDevicesFn mcc_mtl_copy_devices;
static id *mcc_mtl_encinfo_key;
static int mcc_mtl_tried;
static int mcc_mtl_ok;

static const char *const mcc_mtl_foundation[] = {
		"/System/Library/Frameworks/Foundation.framework/Foundation",
		"Foundation.framework/Foundation", NULL};

static const char *const mcc_mtl_metal[] = {
		"/System/Library/Frameworks/Metal.framework/Metal",
		"Metal.framework/Metal", NULL};

typedef struct MtlSize {
	unsigned long w, h, d;
} MtlSize;

typedef struct MccGpu {
	int tried;
	int ok;
	id dev;
	id queue;
	id cbdesc;
	id copts;
	char name[256];
	unsigned long maxbuf;
	unsigned long maxthreads;
	long dispatches;
	long lanes;
	long stranded;
	int fault;
	int fault_encoder;
	long fault_code;
	long faults[MCC_MTL_FAULT_N];
	int f64;
} MccGpu;

typedef struct MtlPipe {
	uint64_t key;
	int len;
	id pso;
} MtlPipe;

static MccGpu mcc_gpu;

static MtlPipe mcc_mtl_cache[MCC_MTL_CACHE_MAX];
static int mcc_mtl_cache_n;
static int mcc_mtl_cache_next;

static id mtl_send(id r, const char *sel) {
	return ((id (*)(id, SEL))objc_msgSend)(r, sel_registerName(sel));
}

static void mtl_send_v(id r, const char *sel) {
	((void (*)(id, SEL))objc_msgSend)(r, sel_registerName(sel));
}

static void mtl_release(id r) {
	if (r)
		mtl_send_v(r, "release");
}

static id mtl_str(const char *s, int len) {
	id o = mtl_send((id)objc_getClass("NSString"), "alloc");
	if (!o)
		return 0;
	return ((id (*)(id, SEL, const void *, unsigned long, unsigned long))
							objc_msgSend)(o, sel_registerName("initWithBytes:length:encoding:"),
														s, (unsigned long)len, MCC_MTL_UTF8);
}

static const char *mtl_utf8(id s) {
	if (!s)
		return "(null)";
	return ((const char *(*)(id, SEL))objc_msgSend)(s,
																								 sel_registerName("UTF8String"));
}

static int mtl_diag(void) { return getenv("MCC_AST_EVAL_LADDER_GPU_DIAG") != 0; }

static int mtl_responds(id r, const char *sel) {
	return ((signed char (*)(id, SEL, SEL))objc_msgSend)(
						 r, sel_registerName("respondsToSelector:"), sel_registerName(sel)) !=
				 0;
}

static unsigned long mtl_ulong(id r, const char *sel, unsigned long dflt) {
	if (!r || !mtl_responds(r, sel))
		return dflt;
	return ((unsigned long (*)(id, SEL))objc_msgSend)(r, sel_registerName(sel));
}

static int mtl_bool(id r, const char *sel, int dflt) {
	if (!r || !mtl_responds(r, sel))
		return dflt;
	return ((signed char (*)(id, SEL))objc_msgSend)(r, sel_registerName(sel)) != 0;
}

static void *mtl_dlopen_any(const char *const *names) {
	void *h = NULL;
	int i;
	for (i = 0; !h && names[i]; i++)
		h = host_dlopen(names[i]);
	return h;
}

static int mcc_mtl_load(void) {
	void *h;
	const char *over;

	if (mcc_mtl_tried)
		return mcc_mtl_ok;
	mcc_mtl_tried = 1;
	mtl_dlopen_any(mcc_mtl_foundation);
	over = getenv("MCC_METAL_LIB");
	h = (over && *over) ? host_dlopen(over) : mtl_dlopen_any(mcc_mtl_metal);
	if (!h) {
		if (mtl_diag())
			fprintf(stderr, "[ladder-gpu] no Metal framework (%s)\n", host_dlerror());
		return 0;
	}
	mcc_mtl_create_device =
			(MccMtlCreateDeviceFn)host_dlsym(h, "MTLCreateSystemDefaultDevice");
	if (!mcc_mtl_create_device) {
		if (mtl_diag())
			fprintf(stderr, "[ladder-gpu] missing symbol MTLCreateSystemDefaultDevice\n");
		return 0;
	}
	mcc_mtl_copy_devices = (MccMtlCopyDevicesFn)host_dlsym(h, "MTLCopyAllDevices");
	mcc_mtl_encinfo_key =
			(id *)host_dlsym(h, "MTLCommandBufferEncoderInfoErrorKey");
	mcc_objc_getClass = (MccObjcGetClassFn)host_dlsym_process("objc_getClass");
	mcc_sel_registerName = (MccObjcSelFn)host_dlsym_process("sel_registerName");
	mcc_objc_msgSend = host_dlsym_process("objc_msgSend");
	if (!mcc_objc_getClass || !mcc_sel_registerName || !mcc_objc_msgSend) {
		void *lo = host_dlopen("/usr/lib/libobjc.A.dylib");
		if (lo) {
			mcc_objc_getClass = (MccObjcGetClassFn)host_dlsym(lo, "objc_getClass");
			mcc_sel_registerName = (MccObjcSelFn)host_dlsym(lo, "sel_registerName");
			mcc_objc_msgSend = host_dlsym(lo, "objc_msgSend");
		}
	}
	if (!mcc_objc_getClass || !mcc_sel_registerName || !mcc_objc_msgSend) {
		if (mtl_diag())
			fprintf(stderr, "[ladder-gpu] no Objective-C runtime in the process\n");
		return 0;
	}
	mcc_fe_get = (MccFeGetFn)host_dlsym_process("fegetenv");
	mcc_fe_set = (MccFeSetFn)host_dlsym_process("fesetenv");
	if (!mcc_fe_get || !mcc_fe_set) {
		void *lm = host_dlopen("libSystem.B.dylib");
		if (lm) {
			mcc_fe_get = (MccFeGetFn)host_dlsym(lm, "fegetenv");
			mcc_fe_set = (MccFeSetFn)host_dlsym(lm, "fesetenv");
		}
	}
	if (!mcc_fe_get || !mcc_fe_set) {
		if (mtl_diag())
			fprintf(stderr, "[ladder-gpu] no fegetenv/fesetenv in the process\n");
		return 0;
	}
	mcc_mtl_ok = 1;
	return 1;
}

static void mtl_report_err(const char *what, id err) {
	if (!mtl_diag())
		return;
	fprintf(stderr, "[ladder-gpu] %s failed: %s\n", what,
					err ? mtl_utf8(mtl_send(err, "localizedDescription")) : "(no error)");
}

static int mtl_classify(const char *d, long code) {
	if (d) {
		if (strstr(d, "InnocentVictim"))
			return MCC_MTL_FAULT_INNOCENT;
		if (strstr(d, "PageFault"))
			return MCC_MTL_FAULT_PAGEFAULT;
		if (strstr(d, "ImpactingInteractivity"))
			return MCC_MTL_FAULT_WATCHDOG;
		if (strstr(d, "Hang"))
			return MCC_MTL_FAULT_HANG;
		if (strstr(d, "Timeout"))
			return MCC_MTL_FAULT_TIMEOUT;
		if (strstr(d, "OutOfMemory"))
			return MCC_MTL_FAULT_OOM;
	}
	switch (code) {
	case 2:
		return MCC_MTL_FAULT_TIMEOUT;
	case 3:
		return MCC_MTL_FAULT_PAGEFAULT;
	case 4:
	case 11:
		return MCC_MTL_FAULT_DEVICE_LOST;
	case 8:
		return MCC_MTL_FAULT_OOM;
	}
	return MCC_MTL_FAULT_UNCLASSIFIED;
}

static int mtl_encoder_state(id err) {
	id ui, arr, e;
	unsigned long n, i;
	int worst = 0;

	if (!err || !mcc_mtl_encinfo_key || !*mcc_mtl_encinfo_key)
		return 0;
	ui = mtl_send(err, "userInfo");
	if (!ui)
		return 0;
	arr = ((id (*)(id, SEL, id))objc_msgSend)(ui, sel_registerName("objectForKey:"),
																						*mcc_mtl_encinfo_key);
	if (!arr)
		return 0;
	n = ((unsigned long (*)(id, SEL))objc_msgSend)(arr, sel_registerName("count"));
	for (i = 0; i < n; i++) {
		int st;
		e = ((id (*)(id, SEL, unsigned long))objc_msgSend)(
				arr, sel_registerName("objectAtIndex:"), i);
		st = (int)((long (*)(id, SEL))objc_msgSend)(e, sel_registerName("errorState"));
		if (st > worst)
			worst = st;
	}
	return worst;
}

static void mtl_fault(int cls, const char *what, id err) {
	const char *d = err ? mtl_utf8(mtl_send(err, "localizedDescription")) : NULL;
	long code =
			err ? ((long (*)(id, SEL))objc_msgSend)(err, sel_registerName("code")) : 0;

	if (cls < 0)
		cls = mtl_classify(d, code);
	mcc_gpu.fault = cls;
	mcc_gpu.fault_code = code;
	mcc_gpu.fault_encoder = mtl_encoder_state(err);
	mcc_gpu.faults[cls]++;
	fprintf(stderr,
					"[ladder-gpu] device-fault class=%s at=%s code=%ld encoder-state=%d "
					"ours=%d count=%ld\n",
					mcc_mtl_fault_names[cls], what, code, mcc_gpu.fault_encoder,
					mcc_gpu.fault_encoder == MCC_MTL_ENC_FAULTED, mcc_gpu.faults[cls]);
	if (mtl_diag())
		fprintf(stderr, "[ladder-gpu] %s failed: %s\n", what, d ? d : "(no error)");
}

int mcc_gpu_fault(const char **name, int *encoder_state, long *code) {
	if (name)
		*name = mcc_mtl_fault_names[mcc_gpu.fault];
	if (encoder_state)
		*encoder_state = mcc_gpu.fault_encoder;
	if (code)
		*code = mcc_gpu.fault_code;
	return mcc_gpu.fault;
}

long mcc_gpu_fault_count(int cls) {
	return (cls >= 0 && cls < MCC_MTL_FAULT_N) ? mcc_gpu.faults[cls] : 0;
}

static long mtl_score(id d) {
	long s = 0;
	if (mtl_bool(d, "hasUnifiedMemory", 0))
		s += 8;
	if (!mtl_bool(d, "isLowPower", 0))
		s += 4;
	if (mtl_bool(d, "isHeadless", 0))
		s += 2;
	if (!mtl_bool(d, "isRemovable", 0))
		s += 1;
	return s;
}

static id mtl_pick_device(void) {
	long want = mcc_env_num("MCC_GPU_DEVICE", 0), bestsc = -1;
	unsigned long n, i, bestbuf = 0;
	id arr, best = 0;

	arr = mcc_mtl_copy_devices ? mcc_mtl_copy_devices() : 0;
	if (!arr) {
		if (want)
			fprintf(stderr, "[ladder-gpu] MCC_GPU_DEVICE=%ld but MTLCopyAllDevices is "
											"unavailable\n",
							want);
		return want ? 0 : mcc_mtl_create_device();
	}
	n = ((unsigned long (*)(id, SEL))objc_msgSend)(arr, sel_registerName("count"));
	if (!n && !want) {
		mtl_release(arr);
		return mcc_mtl_create_device();
	}
	for (i = 0; i < n; i++) {
		id d = ((id (*)(id, SEL, unsigned long))objc_msgSend)(
				arr, sel_registerName("objectAtIndex:"), i);
		unsigned long mb = mtl_ulong(d, "maxBufferLength", 0);
		long sc = mtl_score(d);
		if (mtl_diag())
			fprintf(stderr,
							"[ladder-gpu] device %lu/%lu %s maxBufferLength=%lu score=%ld\n",
							i + 1, n, mtl_utf8(mtl_send(d, "name")), mb, sc);
		if (want) {
			if ((unsigned long)want == i + 1)
				best = d;
			continue;
		}
		if (sc > bestsc || (sc == bestsc && mb > bestbuf)) {
			best = d;
			bestsc = sc;
			bestbuf = mb;
		}
	}
	if (want && !best)
		fprintf(stderr,
						"[ladder-gpu] MCC_GPU_DEVICE=%ld out of range, %lu device(s) "
						"present (1-based)\n",
						want, n);
	if (best)
		mtl_send(best, "retain");
	mtl_release(arr);
	return best;
}

static int mcc_gpu_init(void) {
	id pool, cls;
	MtlSize mt;

	if (mcc_gpu_closing)
		return 0;
	if (mcc_gpu.tried)
		return mcc_gpu.ok;
	mcc_gpu.tried = 1;
	if (!mcc_mtl_load())
		return 0;
	pool = mtl_send(mtl_send((id)objc_getClass("NSAutoreleasePool"), "alloc"),
									"init");
	mcc_gpu.dev = mtl_pick_device();
	if (!mcc_gpu.dev) {
		if (mtl_diag())
			fprintf(stderr, "[ladder-gpu] no Metal device selected\n");
		mtl_send_v(pool, "drain");
		return 0;
	}
	snprintf(mcc_gpu.name, sizeof mcc_gpu.name, "%s",
					 mtl_utf8(mtl_send(mcc_gpu.dev, "name")));
	mcc_gpu.maxbuf = mtl_ulong(mcc_gpu.dev, "maxBufferLength", 0);
	mt.w = 0;
	if (mtl_responds(mcc_gpu.dev, "maxThreadsPerThreadgroup"))
		mt = ((MtlSize(*)(id, SEL))objc_msgSend)(
				mcc_gpu.dev, sel_registerName("maxThreadsPerThreadgroup"));
	mcc_gpu.maxthreads = mt.w;
	if (mcc_gpu.maxbuf < MCC_MTL_MIN_BUFFER ||
			mcc_gpu.maxthreads < MCC_GPU_LOCAL_SIZE) {
		fprintf(stderr,
						"[ladder-gpu] refusing device %s: maxBufferLength=%lu (need %lu) "
						"maxThreadsPerThreadgroup=%lu (need %d)\n",
						mcc_gpu.name, mcc_gpu.maxbuf, MCC_MTL_MIN_BUFFER, mcc_gpu.maxthreads,
						MCC_GPU_LOCAL_SIZE);
		mtl_release(mcc_gpu.dev);
		mcc_gpu.dev = 0;
		mtl_send_v(pool, "drain");
		return 0;
	}
	mcc_gpu.queue = mtl_send(mcc_gpu.dev, "newCommandQueue");
	if (!mcc_gpu.queue) {
		if (mtl_diag())
			fprintf(stderr, "[ladder-gpu] newCommandQueue returned nil\n");
		mtl_release(mcc_gpu.dev);
		mcc_gpu.dev = 0;
		mtl_send_v(pool, "drain");
		return 0;
	}
	cls = (id)objc_getClass("MTLCommandBufferDescriptor");
	if (cls)
		mcc_gpu.cbdesc = mtl_send(mtl_send(cls, "alloc"), "init");
	if (mcc_gpu.cbdesc)
		((void (*)(id, SEL, unsigned long))objc_msgSend)(
				mcc_gpu.cbdesc, sel_registerName("setErrorOptions:"),
				MCC_MTL_ERR_ENCODER_STATUS);
	cls = (id)objc_getClass("MTLCompileOptions");
	if (cls)
		mcc_gpu.copts = mtl_send(mtl_send(cls, "alloc"), "init");
	if (mcc_gpu.copts) {
		if (mtl_responds(mcc_gpu.copts, "setMathMode:"))
			((void (*)(id, SEL, long))objc_msgSend)(
					mcc_gpu.copts, sel_registerName("setMathMode:"), MCC_MTL_MATH_SAFE);
		else if (mtl_responds(mcc_gpu.copts, "setFastMathEnabled:"))
			((void (*)(id, SEL, signed char))objc_msgSend)(
					mcc_gpu.copts, sel_registerName("setFastMathEnabled:"), 0);
		else {
			mtl_release(mcc_gpu.copts);
			mcc_gpu.copts = 0;
		}
	}
	if (!mcc_gpu.copts) {
		fprintf(stderr, "[ladder-gpu] refusing device %s: cannot disable fast math\n",
						mcc_gpu.name);
		mtl_release(mcc_gpu.dev);
		mcc_gpu.dev = 0;
		mtl_send_v(pool, "drain");
		return 0;
	}
	if (mtl_diag())
		fprintf(stderr,
						"[ladder-gpu] init ok dev=%s maxBufferLength=%lu "
						"maxThreadsPerThreadgroup=%lu encoder-status=%d\n",
						mcc_gpu.name, mcc_gpu.maxbuf, mcc_gpu.maxthreads,
						mcc_gpu.cbdesc != 0);
	mtl_send_v(pool, "drain");
	mcc_gpu.ok = 1;
	mcc_gpu.f64 = 1;
	return 1;
}

/* The process-resident shared region; see mtl_bind_mem below. Declared here
 * because mcc_gpu_quiesce releases it. */
static id mcc_mtl_mem;
static void *mcc_mtl_pmem;
static unsigned long mcc_mtl_memsz;

static id mcc_mtl_bin, mcc_mtl_bout;
static void *mcc_mtl_pin, *mcc_mtl_pout;
static unsigned long mcc_mtl_binsz, mcc_mtl_boutsz;

void mcc_gpu_quiesce(void) {
	int i;
	MCC_GPU_LOCK();
	mcc_gpu_closing = 1;
	for (i = 0; i < mcc_mtl_cache_n; i++)
		mtl_release(mcc_mtl_cache[i].pso);
	mcc_mtl_cache_n = 0;
	/* Safe to drop here: the Metal dispatch is synchronous (waitUntilCompleted),
	 * so no command buffer can still own it. */
	mtl_release(mcc_mtl_mem);
	mcc_mtl_mem = 0;
	mcc_mtl_pmem = NULL;
	mcc_mtl_memsz = 0;
	mtl_release(mcc_mtl_bin);
	mcc_mtl_bin = 0;
	mcc_mtl_pin = NULL;
	mcc_mtl_binsz = 0;
	mtl_release(mcc_mtl_bout);
	mcc_mtl_bout = 0;
	mcc_mtl_pout = NULL;
	mcc_mtl_boutsz = 0;
	MCC_GPU_UNLOCK();
}

int mcc_gpu_reopen(void) {
	int ok;
	MCC_GPU_LOCK();
	mcc_gpu_closing = 0;
	ok = mcc_gpu.ok;
	MCC_GPU_UNLOCK();
	return ok;
}

/* Metal holds a single device today; the routing API (T-win-50022 slice 1b, a
   Vulkan-side capability) is stubbed here so cross-platform callers link. Holding
   more than one Metal device is mac-arm64 territory. */
int mcc_gpu_device_count(void) { return mcc_gpu_init() ? 1 : 0; }
int mcc_gpu_route(int i) { return i == 0 ? 0 : -1; }

static uint64_t mtl_key(const char *s, int len) {
	uint64_t h = 0xcbf29ce484222325u;
	int i;
	for (i = 0; i < len; i++) {
		h ^= (unsigned char)s[i];
		h *= 0x100000001b3u;
	}
	return h;
}

static id mtl_pipeline(const char *src, int len) {
	uint64_t key = mtl_key(src, len);
	id err = 0, str, lib, fn, fname, pso;
	int i;

	for (i = 0; i < mcc_mtl_cache_n; i++)
		if (mcc_mtl_cache[i].key == key && mcc_mtl_cache[i].len == len)
			return mcc_mtl_cache[i].pso;

	str = mtl_str(src, len);
	if (!str)
		return 0;
	lib = ((id (*)(id, SEL, id, id, id *))objc_msgSend)(
			mcc_gpu.dev, sel_registerName("newLibraryWithSource:options:error:"), str,
			mcc_gpu.copts, &err);
	mtl_release(str);
	if (!lib) {
		mtl_report_err("newLibraryWithSource", err);
		return 0;
	}
	fname = mtl_str("mcc_main", 8);
	fn = ((id (*)(id, SEL, id))objc_msgSend)(
			lib, sel_registerName("newFunctionWithName:"), fname);
	mtl_release(fname);
	if (!fn) {
		mtl_report_err("newFunctionWithName", 0);
		mtl_release(lib);
		return 0;
	}
	err = 0;
	pso = ((id (*)(id, SEL, id, id *))objc_msgSend)(
			mcc_gpu.dev,
			sel_registerName("newComputePipelineStateWithFunction:error:"), fn, &err);
	mtl_release(fn);
	mtl_release(lib);
	if (!pso) {
		mtl_report_err("newComputePipelineStateWithFunction", err);
		return 0;
	}
	if (mtl_ulong(pso, "maxTotalThreadsPerThreadgroup", MCC_GPU_LOCAL_SIZE) <
			MCC_GPU_LOCAL_SIZE) {
		fprintf(stderr,
						"[ladder-gpu] refusing pipeline: maxTotalThreadsPerThreadgroup=%lu "
						"below the %d this layer dispatches\n",
						mtl_ulong(pso, "maxTotalThreadsPerThreadgroup", 0),
						MCC_GPU_LOCAL_SIZE);
		mtl_release(pso);
		return 0;
	}
	if (mcc_mtl_cache_n < MCC_MTL_CACHE_MAX) {
		mcc_mtl_cache[mcc_mtl_cache_n].key = key;
		mcc_mtl_cache[mcc_mtl_cache_n].len = len;
		mcc_mtl_cache[mcc_mtl_cache_n].pso = pso;
		mcc_mtl_cache_n++;
		return pso;
	}
	mtl_release(mcc_mtl_cache[mcc_mtl_cache_next].pso);
	mcc_mtl_cache[mcc_mtl_cache_next].key = key;
	mcc_mtl_cache[mcc_mtl_cache_next].len = len;
	mcc_mtl_cache[mcc_mtl_cache_next].pso = pso;
	mcc_mtl_cache_next = (mcc_mtl_cache_next + 1) % MCC_MTL_CACHE_MAX;
	return pso;
}

static id mtl_buffer(unsigned long len, void **map) {
	id b = ((id (*)(id, SEL, unsigned long, unsigned long))objc_msgSend)(
			mcc_gpu.dev, sel_registerName("newBufferWithLength:options:"), len, 0);
	if (!b)
		return 0;
	*map = ((void *(*)(id, SEL))objc_msgSend)(b, sel_registerName("contents"));
	if (!*map) {
		mtl_release(b);
		return 0;
	}
	return b;
}

static int mtl_bind_buffers(unsigned long inlen, unsigned long outlen) {
	if (inlen > mcc_mtl_binsz) {
		mtl_release(mcc_mtl_bin);
		mcc_mtl_bin = mtl_buffer(inlen, &mcc_mtl_pin);
		if (!mcc_mtl_bin) {
			mcc_mtl_binsz = 0;
			mcc_mtl_pin = NULL;
			return 0;
		}
		mcc_mtl_binsz = inlen;
	}
	if (outlen > mcc_mtl_boutsz) {
		mtl_release(mcc_mtl_bout);
		mcc_mtl_bout = mtl_buffer(outlen, &mcc_mtl_pout);
		if (!mcc_mtl_bout) {
			mcc_mtl_boutsz = 0;
			mcc_mtl_pout = NULL;
			return 0;
		}
		mcc_mtl_boutsz = outlen;
	}
	return 1;
}

/* Set only for the duration of a frame dispatch, under the same lock that
 * serialises everything else here. */
static int32_t *mcc_gpu_rw_back;

static int mcc_gpu_dispatch_locked2(const char *src, int len, const int32_t *in,
																		int ntuple, int nlive, int32_t *out,
																		int reuse_in) {
	id pool, pso, cb, enc;
	MtlSize grid, tg;
	int cap = ((ntuple + MCC_GPU_LOCAL_SIZE - 1) / MCC_GPU_LOCAL_SIZE) * MCC_GPU_LOCAL_SIZE;
	unsigned long inlen = (unsigned long)cap * nlive * MCC_GPU_IN_SLOTS * 4;
	unsigned long outlen = (unsigned long)cap * MCC_GPU_OUT_SLOTS * 4;
	int rc = 0;

	if (!mcc_gpu_init())
		return 0;
	if (inlen > mcc_gpu.maxbuf || outlen > mcc_gpu.maxbuf) {
		mtl_fault(MCC_MTL_FAULT_OVER_LIMIT, "newBufferWithLength", 0);
		return 0;
	}
	pool = mtl_send(mtl_send((id)objc_getClass("NSAutoreleasePool"), "alloc"),
									"init");
	pso = mtl_pipeline(src, len);
	if (!pso)
		goto done;
	if (!mtl_bind_buffers(inlen, outlen))
		goto done;
	if (!reuse_in) {
		memcpy(mcc_mtl_pin, in, (size_t)ntuple * nlive * MCC_GPU_IN_SLOTS * 4);
		if (cap > ntuple)
			memset((char *)mcc_mtl_pin +
								 (size_t)ntuple * nlive * MCC_GPU_IN_SLOTS * 4,
						 0, (size_t)(cap - ntuple) * nlive * MCC_GPU_IN_SLOTS * 4);
	}

	cb = mcc_gpu.cbdesc
					 ? ((id (*)(id, SEL, id))objc_msgSend)(
								 mcc_gpu.queue, sel_registerName("commandBufferWithDescriptor:"),
								 mcc_gpu.cbdesc)
					 : mtl_send(mcc_gpu.queue, "commandBuffer");
	enc = cb ? mtl_send(cb, "computeCommandEncoder") : 0;
	if (!enc) {
		mtl_fault(MCC_MTL_FAULT_ENCODE, "computeCommandEncoder", 0);
		goto done;
	}
	((void (*)(id, SEL, id))objc_msgSend)(
			enc, sel_registerName("setComputePipelineState:"), pso);
	((void (*)(id, SEL, id, unsigned long, unsigned long))objc_msgSend)(
			enc, sel_registerName("setBuffer:offset:atIndex:"), mcc_mtl_bin, 0, 0);
	((void (*)(id, SEL, id, unsigned long, unsigned long))objc_msgSend)(
			enc, sel_registerName("setBuffer:offset:atIndex:"), mcc_mtl_bout, 0, 1);
	if (mcc_mtl_mem)
		((void (*)(id, SEL, id, unsigned long, unsigned long))objc_msgSend)(
				enc, sel_registerName("setBuffer:offset:atIndex:"), mcc_mtl_mem, 0, 2);
	grid.w = (unsigned long)(cap / MCC_GPU_LOCAL_SIZE);
	grid.h = 1;
	grid.d = 1;
	tg.w = MCC_GPU_LOCAL_SIZE;
	tg.h = 1;
	tg.d = 1;
	((void (*)(id, SEL, MtlSize, MtlSize))objc_msgSend)(
			enc, sel_registerName("dispatchThreadgroups:threadsPerThreadgroup:"), grid,
			tg);
	mtl_send_v(enc, "endEncoding");
	mtl_send_v(cb, "commit");
	mtl_send_v(cb, "waitUntilCompleted");
	if (((unsigned long (*)(id, SEL))objc_msgSend)(
					cb, sel_registerName("status")) != MCC_MTL_CB_COMPLETED) {
		mtl_fault(-1, "command buffer", mtl_send(cb, "error"));
		goto done;
	}
	mcc_gpu.fault = MCC_MTL_FAULT_NONE;
	/* `out` is NULL whenever the caller wants only the frame back --
	 * mcc_gpu_dispatch_rw passes NULL, and mcc_slice_run_frame_gpu passes a NULL
	 * ob when neither retval nor retdef was asked for. The Vulkan twin has always
	 * guarded this; here it was unreachable only because dispatch_rw2 bailed on
	 * !mcc_gpu_rw_supported(), which now returns 1. */
	if (out)
		memcpy(out, mcc_mtl_pout, (size_t)ntuple * MCC_GPU_OUT_SLOTS * 4);
	/* The frame copy-back. Buffer 0 is MTLResourceStorageModeShared (options 0
	 * in mtl_buffer), so `contents` is CPU-coherent the moment
	 * waitUntilCompleted returns -- no blit, no synchronizeResource:. If those
	 * options ever become Managed this reads stale bytes. */
	if (mcc_gpu_rw_back)
		memcpy(mcc_gpu_rw_back, mcc_mtl_pin,
					 (size_t)ntuple * nlive * MCC_GPU_IN_SLOTS * 4);
	mcc_gpu_ctr.dispatches++;
	mcc_gpu_ctr.lanes += ntuple;
	rc = 1;

done:
	mtl_send_v(pool, "drain");
	return rc;
}

static int mcc_gpu_backend_load(void) { return mcc_mtl_load(); }

#define MCC_GPU_CODE_PTR(p) ((const char *)(p))
#define MCC_GPU_IN_IS_RESIDENT 1

static int mcc_gpu_rw_supported(void) { return 1; }

static void mcc_gpu_rw_arm(int32_t *p) { mcc_gpu_rw_back = p; }

/* Same contract as MCC_VK_MEM_DEFAULT on the Vulkan arm: one host-mapped region
 * every lane sees, offset 0 reserved as NULL, coherent at command-buffer
 * granularity only -- seed before submit, drain after, never during.
 *
 * Unlike bin/bout, which mcc_gpu_dispatch_locked creates and releases per
 * dispatch, this one is process-resident: suite_mem writes a byte, calls back in
 * and requires the identical pointer with the byte intact. */
#define MCC_MTL_MEM_DEFAULT (1u << 20)

static int mtl_bind_mem(unsigned long want) {
	id b;
	void *p = NULL;
	if (mcc_mtl_mem && want <= mcc_mtl_memsz)
		return 1;
	if (want > mcc_gpu.maxbuf) {
		mtl_fault(MCC_MTL_FAULT_OVER_LIMIT, "newBufferWithLength", 0);
		return 0;
	}
	b = mtl_buffer(want, &p);
	if (!b)
		return 0;
	memset(p, 0, (size_t)want);
	if (mcc_mtl_mem) {
		memcpy(p, mcc_mtl_pmem, (size_t)mcc_mtl_memsz);
		mtl_release(mcc_mtl_mem);
	}
	mcc_mtl_mem = b;
	mcc_mtl_pmem = p;
	mcc_mtl_memsz = want;
	return 1;
}

static int mcc_gpu_mem_backend(void **base, unsigned long *size) {
	if (!mcc_gpu_init() || !mtl_bind_mem(MCC_MTL_MEM_DEFAULT))
		return 0;
	if (base)
		*base = mcc_mtl_pmem;
	if (size)
		*size = mcc_mtl_memsz;
	return 1;
}

static unsigned long mcc_gpu_host_import_align_backend(const char **why) {
	if (why)
		*why = "the Metal arm cannot import a host pointer: the encoder binds "
					 "buffers 0 and 1 only and both MSL kernels declare two, so there is "
					 "no binding for an imported region to arrive on";
	return 0;
}

static int mcc_gpu_mem_import_backend(void *base, unsigned long size) {
	(void)base;
	(void)size;
	return 0;
}

#else /* !MCC_GPU_LANG_MSL */

#if MCC_HOST_WIN32
#define VKAPI_PTR __stdcall
#else
#define VKAPI_PTR
#endif

#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFULL
#define MCC_VK_HANDLE(object) typedef struct object##_T *object;
#define VK_NULL_HANDLE ((void *)0)
#else
#define MCC_VK_HANDLE(object) typedef uint64_t object;
#define VK_NULL_HANDLE 0ULL
#endif

#define MCC_VK_DISPATCHABLE(object) typedef struct object##_T *object;

MCC_VK_DISPATCHABLE(VkInstance)
MCC_VK_DISPATCHABLE(VkPhysicalDevice)
MCC_VK_DISPATCHABLE(VkDevice)
MCC_VK_DISPATCHABLE(VkQueue)
MCC_VK_DISPATCHABLE(VkCommandBuffer)

MCC_VK_HANDLE(VkBuffer)
MCC_VK_HANDLE(VkBufferView)
MCC_VK_HANDLE(VkCommandPool)
MCC_VK_HANDLE(VkDescriptorPool)
MCC_VK_HANDLE(VkDescriptorSet)
MCC_VK_HANDLE(VkDescriptorSetLayout)
MCC_VK_HANDLE(VkDeviceMemory)
MCC_VK_HANDLE(VkFence)
MCC_VK_HANDLE(VkImageView)
MCC_VK_HANDLE(VkPipeline)
MCC_VK_HANDLE(VkPipelineCache)
MCC_VK_HANDLE(VkPipelineCache)
MCC_VK_HANDLE(VkPipelineLayout)
MCC_VK_HANDLE(VkSampler)
MCC_VK_HANDLE(VkSemaphore)
MCC_VK_HANDLE(VkShaderModule)

typedef uint32_t VkFlags;
typedef uint32_t VkBool32;
typedef uint64_t VkDeviceSize;

typedef VkFlags VkBufferCreateFlags;
typedef VkFlags VkBufferUsageFlags;
typedef VkFlags VkCommandBufferUsageFlags;
typedef VkFlags VkCommandPoolCreateFlags;
typedef VkFlags VkDescriptorPoolCreateFlags;
typedef VkFlags VkDescriptorSetLayoutCreateFlags;
typedef VkFlags VkDeviceCreateFlags;
typedef VkFlags VkDeviceQueueCreateFlags;
typedef VkFlags VkFenceCreateFlags;
typedef VkFlags VkInstanceCreateFlags;
typedef VkFlags VkMemoryHeapFlags;
typedef VkFlags VkMemoryMapFlags;
typedef VkFlags VkMemoryPropertyFlags;
typedef VkFlags VkPipelineCreateFlags;
typedef VkFlags VkPipelineLayoutCreateFlags;
typedef VkFlags VkPipelineShaderStageCreateFlags;
typedef VkFlags VkPipelineStageFlags;
typedef VkFlags VkQueryControlFlags;
typedef VkFlags VkQueryPipelineStatisticFlags;
typedef VkFlags VkQueueFlags;
typedef VkFlags VkSampleCountFlags;
typedef VkFlags VkShaderModuleCreateFlags;
typedef VkFlags VkShaderStageFlags;

typedef struct VkAllocationCallbacks VkAllocationCallbacks;
typedef struct VkCommandBufferInheritanceInfo VkCommandBufferInheritanceInfo;
typedef struct VkCopyDescriptorSet VkCopyDescriptorSet;
typedef struct VkDescriptorImageInfo VkDescriptorImageInfo;
typedef struct VkPhysicalDeviceFeatures {
	VkBool32 robustBufferAccess;
	VkBool32 fullDrawIndexUint32;
	VkBool32 imageCubeArray;
	VkBool32 independentBlend;
	VkBool32 geometryShader;
	VkBool32 tessellationShader;
	VkBool32 sampleRateShading;
	VkBool32 dualSrcBlend;
	VkBool32 logicOp;
	VkBool32 multiDrawIndirect;
	VkBool32 drawIndirectFirstInstance;
	VkBool32 depthClamp;
	VkBool32 depthBiasClamp;
	VkBool32 fillModeNonSolid;
	VkBool32 depthBounds;
	VkBool32 wideLines;
	VkBool32 largePoints;
	VkBool32 alphaToOne;
	VkBool32 multiViewport;
	VkBool32 samplerAnisotropy;
	VkBool32 textureCompressionETC2;
	VkBool32 textureCompressionASTC_LDR;
	VkBool32 textureCompressionBC;
	VkBool32 occlusionQueryPrecise;
	VkBool32 pipelineStatisticsQuery;
	VkBool32 vertexPipelineStoresAndAtomics;
	VkBool32 fragmentStoresAndAtomics;
	VkBool32 shaderTessellationAndGeometryPointSize;
	VkBool32 shaderImageGatherExtended;
	VkBool32 shaderStorageImageExtendedFormats;
	VkBool32 shaderStorageImageMultisample;
	VkBool32 shaderStorageImageReadWithoutFormat;
	VkBool32 shaderStorageImageWriteWithoutFormat;
	VkBool32 shaderUniformBufferArrayDynamicIndexing;
	VkBool32 shaderSampledImageArrayDynamicIndexing;
	VkBool32 shaderStorageBufferArrayDynamicIndexing;
	VkBool32 shaderStorageImageArrayDynamicIndexing;
	VkBool32 shaderClipDistance;
	VkBool32 shaderCullDistance;
	VkBool32 shaderFloat64;
	VkBool32 shaderInt64;
	VkBool32 shaderInt16;
	VkBool32 shaderResourceResidency;
	VkBool32 shaderResourceMinLod;
	VkBool32 sparseBinding;
	VkBool32 sparseResidencyBuffer;
	VkBool32 sparseResidencyImage2D;
	VkBool32 sparseResidencyImage3D;
	VkBool32 sparseResidency2Samples;
	VkBool32 sparseResidency4Samples;
	VkBool32 sparseResidency8Samples;
	VkBool32 sparseResidency16Samples;
	VkBool32 sparseResidencyAliased;
	VkBool32 variableMultisampleRate;
	VkBool32 inheritedQueries;
} VkPhysicalDeviceFeatures;
typedef struct VkPushConstantRange VkPushConstantRange;
typedef struct VkSpecializationInfo VkSpecializationInfo;

typedef enum VkResult {
	VK_SUCCESS = 0,
	VK_NOT_READY = 1,
	VK_TIMEOUT = 2,
	VK_EVENT_SET = 3,
	VK_EVENT_RESET = 4,
	VK_INCOMPLETE = 5,
	VK_ERROR_OUT_OF_HOST_MEMORY = -1,
	VK_ERROR_OUT_OF_DEVICE_MEMORY = -2,
	VK_ERROR_INITIALIZATION_FAILED = -3,
	VK_ERROR_DEVICE_LOST = -4,
	VK_RESULT_MAX_ENUM = 0x7FFFFFFF
} VkResult;

typedef enum VkStructureType {
	VK_STRUCTURE_TYPE_APPLICATION_INFO = 0,
	VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1,
	VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2,
	VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3,
	VK_STRUCTURE_TYPE_SUBMIT_INFO = 4,
	VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5,
	VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8,
	VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO = 17,
	VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO = 12,
	VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO = 16,
	VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO = 18,
	VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO = 29,
	VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO = 30,
	VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO = 32,
	VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO = 33,
	VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO = 34,
	VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET = 35,
	VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39,
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40,
	VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42,
	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 = 1000059001,
	VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO = 1000072000,
	VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT = 1000178000,
	VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT = 1000178001,
	VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT =
			1000178002,
	VK_STRUCTURE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkStructureType;

typedef enum VkPhysicalDeviceType {
	VK_PHYSICAL_DEVICE_TYPE_OTHER = 0,
	VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU = 1,
	VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU = 2,
	VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU = 3,
	VK_PHYSICAL_DEVICE_TYPE_CPU = 4,
	VK_PHYSICAL_DEVICE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkPhysicalDeviceType;

typedef enum VkSharingMode {
	VK_SHARING_MODE_EXCLUSIVE = 0,
	VK_SHARING_MODE_CONCURRENT = 1,
	VK_SHARING_MODE_MAX_ENUM = 0x7FFFFFFF
} VkSharingMode;

typedef enum VkDescriptorType {
	VK_DESCRIPTOR_TYPE_STORAGE_BUFFER = 7,
	VK_DESCRIPTOR_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkDescriptorType;

typedef enum VkCommandBufferLevel {
	VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0,
	VK_COMMAND_BUFFER_LEVEL_SECONDARY = 1,
	VK_COMMAND_BUFFER_LEVEL_MAX_ENUM = 0x7FFFFFFF
} VkCommandBufferLevel;

typedef enum VkPipelineBindPoint {
	VK_PIPELINE_BIND_POINT_GRAPHICS = 0,
	VK_PIPELINE_BIND_POINT_COMPUTE = 1,
	VK_PIPELINE_BIND_POINT_MAX_ENUM = 0x7FFFFFFF
} VkPipelineBindPoint;

typedef enum VkShaderStageFlagBits {
	VK_SHADER_STAGE_COMPUTE_BIT = 0x00000020,
	VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VkShaderStageFlagBits;

#define VK_TRUE 1U
#define VK_FALSE 0U
#define VK_WHOLE_SIZE (~0ULL)
#define VK_UUID_SIZE 16U
#define VK_MAX_MEMORY_TYPES 32U
#define VK_MAX_MEMORY_HEAPS 16U
#define VK_MAX_PHYSICAL_DEVICE_NAME_SIZE 256U

#define VK_MAKE_API_VERSION(variant, major, minor, patch)        \
	((((uint32_t)(variant)) << 29) | (((uint32_t)(major)) << 22) | \
	 (((uint32_t)(minor)) << 12) | ((uint32_t)(patch)))
#define VK_API_VERSION_1_0 VK_MAKE_API_VERSION(0, 1, 0, 0)
#define VK_API_VERSION_1_1 VK_MAKE_API_VERSION(0, 1, 1, 0)

#define VK_QUEUE_COMPUTE_BIT 0x00000002
#define VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT 0x00000002
#define VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 0x00000001
#define VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 0x00000002
#define VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 0x00000004
#define VK_MEMORY_PROPERTY_HOST_CACHED_BIT 0x00000008
#define VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 0x00000020
#define VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 0x00000001

typedef struct VkExtent3D {
	uint32_t width;
	uint32_t height;
	uint32_t depth;
} VkExtent3D;

typedef struct VkApplicationInfo {
	VkStructureType sType;
	const void *pNext;
	const char *pApplicationName;
	uint32_t applicationVersion;
	const char *pEngineName;
	uint32_t engineVersion;
	uint32_t apiVersion;
} VkApplicationInfo;

typedef struct VkInstanceCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkInstanceCreateFlags flags;
	const VkApplicationInfo *pApplicationInfo;
	uint32_t enabledLayerCount;
	const char *const *ppEnabledLayerNames;
	uint32_t enabledExtensionCount;
	const char *const *ppEnabledExtensionNames;
} VkInstanceCreateInfo;

typedef struct VkQueueFamilyProperties {
	VkQueueFlags queueFlags;
	uint32_t queueCount;
	uint32_t timestampValidBits;
	VkExtent3D minImageTransferGranularity;
} VkQueueFamilyProperties;

typedef struct VkPhysicalDeviceLimits {
	uint32_t maxImageDimension1D;
	uint32_t maxImageDimension2D;
	uint32_t maxImageDimension3D;
	uint32_t maxImageDimensionCube;
	uint32_t maxImageArrayLayers;
	uint32_t maxTexelBufferElements;
	uint32_t maxUniformBufferRange;
	uint32_t maxStorageBufferRange;
	uint32_t maxPushConstantsSize;
	uint32_t maxMemoryAllocationCount;
	uint32_t maxSamplerAllocationCount;
	VkDeviceSize bufferImageGranularity;
	VkDeviceSize sparseAddressSpaceSize;
	uint32_t maxBoundDescriptorSets;
	uint32_t maxPerStageDescriptorSamplers;
	uint32_t maxPerStageDescriptorUniformBuffers;
	uint32_t maxPerStageDescriptorStorageBuffers;
	uint32_t maxPerStageDescriptorSampledImages;
	uint32_t maxPerStageDescriptorStorageImages;
	uint32_t maxPerStageDescriptorInputAttachments;
	uint32_t maxPerStageResources;
	uint32_t maxDescriptorSetSamplers;
	uint32_t maxDescriptorSetUniformBuffers;
	uint32_t maxDescriptorSetUniformBuffersDynamic;
	uint32_t maxDescriptorSetStorageBuffers;
	uint32_t maxDescriptorSetStorageBuffersDynamic;
	uint32_t maxDescriptorSetSampledImages;
	uint32_t maxDescriptorSetStorageImages;
	uint32_t maxDescriptorSetInputAttachments;
	uint32_t maxVertexInputAttributes;
	uint32_t maxVertexInputBindings;
	uint32_t maxVertexInputAttributeOffset;
	uint32_t maxVertexInputBindingStride;
	uint32_t maxVertexOutputComponents;
	uint32_t maxTessellationGenerationLevel;
	uint32_t maxTessellationPatchSize;
	uint32_t maxTessellationControlPerVertexInputComponents;
	uint32_t maxTessellationControlPerVertexOutputComponents;
	uint32_t maxTessellationControlPerPatchOutputComponents;
	uint32_t maxTessellationControlTotalOutputComponents;
	uint32_t maxTessellationEvaluationInputComponents;
	uint32_t maxTessellationEvaluationOutputComponents;
	uint32_t maxGeometryShaderInvocations;
	uint32_t maxGeometryInputComponents;
	uint32_t maxGeometryOutputComponents;
	uint32_t maxGeometryOutputVertices;
	uint32_t maxGeometryTotalOutputComponents;
	uint32_t maxFragmentInputComponents;
	uint32_t maxFragmentOutputAttachments;
	uint32_t maxFragmentDualSrcAttachments;
	uint32_t maxFragmentCombinedOutputResources;
	uint32_t maxComputeSharedMemorySize;
	uint32_t maxComputeWorkGroupCount[3];
	uint32_t maxComputeWorkGroupInvocations;
	uint32_t maxComputeWorkGroupSize[3];
	uint32_t subPixelPrecisionBits;
	uint32_t subTexelPrecisionBits;
	uint32_t mipmapPrecisionBits;
	uint32_t maxDrawIndexedIndexValue;
	uint32_t maxDrawIndirectCount;
	float maxSamplerLodBias;
	float maxSamplerAnisotropy;
	uint32_t maxViewports;
	uint32_t maxViewportDimensions[2];
	float viewportBoundsRange[2];
	uint32_t viewportSubPixelBits;
	size_t minMemoryMapAlignment;
	VkDeviceSize minTexelBufferOffsetAlignment;
	VkDeviceSize minUniformBufferOffsetAlignment;
	VkDeviceSize minStorageBufferOffsetAlignment;
	int32_t minTexelOffset;
	uint32_t maxTexelOffset;
	int32_t minTexelGatherOffset;
	uint32_t maxTexelGatherOffset;
	float minInterpolationOffset;
	float maxInterpolationOffset;
	uint32_t subPixelInterpolationOffsetBits;
	uint32_t maxFramebufferWidth;
	uint32_t maxFramebufferHeight;
	uint32_t maxFramebufferLayers;
	VkSampleCountFlags framebufferColorSampleCounts;
	VkSampleCountFlags framebufferDepthSampleCounts;
	VkSampleCountFlags framebufferStencilSampleCounts;
	VkSampleCountFlags framebufferNoAttachmentsSampleCounts;
	uint32_t maxColorAttachments;
	VkSampleCountFlags sampledImageColorSampleCounts;
	VkSampleCountFlags sampledImageIntegerSampleCounts;
	VkSampleCountFlags sampledImageDepthSampleCounts;
	VkSampleCountFlags sampledImageStencilSampleCounts;
	VkSampleCountFlags storageImageSampleCounts;
	uint32_t maxSampleMaskWords;
	VkBool32 timestampComputeAndGraphics;
	float timestampPeriod;
	uint32_t maxClipDistances;
	uint32_t maxCullDistances;
	uint32_t maxCombinedClipAndCullDistances;
	uint32_t discreteQueuePriorities;
	float pointSizeRange[2];
	float lineWidthRange[2];
	float pointSizeGranularity;
	float lineWidthGranularity;
	VkBool32 strictLines;
	VkBool32 standardSampleLocations;
	VkDeviceSize optimalBufferCopyOffsetAlignment;
	VkDeviceSize optimalBufferCopyRowPitchAlignment;
	VkDeviceSize nonCoherentAtomSize;
} VkPhysicalDeviceLimits;

typedef struct VkPhysicalDeviceSparseProperties {
	VkBool32 residencyStandard2DBlockShape;
	VkBool32 residencyStandard2DMultisampleBlockShape;
	VkBool32 residencyStandard3DBlockShape;
	VkBool32 residencyAlignedMipSize;
	VkBool32 residencyNonResidentStrict;
} VkPhysicalDeviceSparseProperties;

typedef struct VkPhysicalDeviceProperties {
	uint32_t apiVersion;
	uint32_t driverVersion;
	uint32_t vendorID;
	uint32_t deviceID;
	VkPhysicalDeviceType deviceType;
	char deviceName[VK_MAX_PHYSICAL_DEVICE_NAME_SIZE];
	uint8_t pipelineCacheUUID[VK_UUID_SIZE];
	VkPhysicalDeviceLimits limits;
	VkPhysicalDeviceSparseProperties sparseProperties;
} VkPhysicalDeviceProperties;

typedef struct VkDeviceQueueCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkDeviceQueueCreateFlags flags;
	uint32_t queueFamilyIndex;
	uint32_t queueCount;
	const float *pQueuePriorities;
} VkDeviceQueueCreateInfo;

typedef struct VkDeviceCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkDeviceCreateFlags flags;
	uint32_t queueCreateInfoCount;
	const VkDeviceQueueCreateInfo *pQueueCreateInfos;
	uint32_t enabledLayerCount;
	const char *const *ppEnabledLayerNames;
	uint32_t enabledExtensionCount;
	const char *const *ppEnabledExtensionNames;
	const VkPhysicalDeviceFeatures *pEnabledFeatures;
} VkDeviceCreateInfo;

typedef struct VkMemoryRequirements {
	VkDeviceSize size;
	VkDeviceSize alignment;
	uint32_t memoryTypeBits;
} VkMemoryRequirements;

typedef struct VkMemoryType {
	VkMemoryPropertyFlags propertyFlags;
	uint32_t heapIndex;
} VkMemoryType;

typedef struct VkMemoryHeap {
	VkDeviceSize size;
	VkMemoryHeapFlags flags;
} VkMemoryHeap;

typedef struct VkPhysicalDeviceMemoryProperties {
	uint32_t memoryTypeCount;
	VkMemoryType memoryTypes[VK_MAX_MEMORY_TYPES];
	uint32_t memoryHeapCount;
	VkMemoryHeap memoryHeaps[VK_MAX_MEMORY_HEAPS];
} VkPhysicalDeviceMemoryProperties;

typedef struct VkBufferCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkBufferCreateFlags flags;
	VkDeviceSize size;
	VkBufferUsageFlags usage;
	VkSharingMode sharingMode;
	uint32_t queueFamilyIndexCount;
	const uint32_t *pQueueFamilyIndices;
} VkBufferCreateInfo;

typedef struct VkMemoryAllocateInfo {
	VkStructureType sType;
	const void *pNext;
	VkDeviceSize allocationSize;
	uint32_t memoryTypeIndex;
} VkMemoryAllocateInfo;

typedef struct VkDescriptorSetLayoutBinding {
	uint32_t binding;
	VkDescriptorType descriptorType;
	uint32_t descriptorCount;
	VkShaderStageFlags stageFlags;
	const VkSampler *pImmutableSamplers;
} VkDescriptorSetLayoutBinding;

typedef struct VkDescriptorSetLayoutCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkDescriptorSetLayoutCreateFlags flags;
	uint32_t bindingCount;
	const VkDescriptorSetLayoutBinding *pBindings;
} VkDescriptorSetLayoutCreateInfo;

typedef struct VkDescriptorPoolSize {
	VkDescriptorType type;
	uint32_t descriptorCount;
} VkDescriptorPoolSize;

typedef struct VkDescriptorPoolCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkDescriptorPoolCreateFlags flags;
	uint32_t maxSets;
	uint32_t poolSizeCount;
	const VkDescriptorPoolSize *pPoolSizes;
} VkDescriptorPoolCreateInfo;

typedef struct VkDescriptorSetAllocateInfo {
	VkStructureType sType;
	const void *pNext;
	VkDescriptorPool descriptorPool;
	uint32_t descriptorSetCount;
	const VkDescriptorSetLayout *pSetLayouts;
} VkDescriptorSetAllocateInfo;

typedef struct VkDescriptorBufferInfo {
	VkBuffer buffer;
	VkDeviceSize offset;
	VkDeviceSize range;
} VkDescriptorBufferInfo;

typedef struct VkWriteDescriptorSet {
	VkStructureType sType;
	const void *pNext;
	VkDescriptorSet dstSet;
	uint32_t dstBinding;
	uint32_t dstArrayElement;
	uint32_t descriptorCount;
	VkDescriptorType descriptorType;
	const VkDescriptorImageInfo *pImageInfo;
	const VkDescriptorBufferInfo *pBufferInfo;
	const VkBufferView *pTexelBufferView;
} VkWriteDescriptorSet;

typedef struct VkShaderModuleCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkShaderModuleCreateFlags flags;
	size_t codeSize;
	const uint32_t *pCode;
} VkShaderModuleCreateInfo;

typedef struct VkPipelineLayoutCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkPipelineLayoutCreateFlags flags;
	uint32_t setLayoutCount;
	const VkDescriptorSetLayout *pSetLayouts;
	uint32_t pushConstantRangeCount;
	const VkPushConstantRange *pPushConstantRanges;
} VkPipelineLayoutCreateInfo;

typedef struct VkPipelineShaderStageCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkPipelineShaderStageCreateFlags flags;
	VkShaderStageFlagBits stage;
	VkShaderModule module;
	const char *pName;
	const VkSpecializationInfo *pSpecializationInfo;
} VkPipelineShaderStageCreateInfo;

typedef struct VkComputePipelineCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkPipelineCreateFlags flags;
	VkPipelineShaderStageCreateInfo stage;
	VkPipelineLayout layout;
	VkPipeline basePipelineHandle;
	int32_t basePipelineIndex;
} VkComputePipelineCreateInfo;

typedef struct VkCommandPoolCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkCommandPoolCreateFlags flags;
	uint32_t queueFamilyIndex;
} VkCommandPoolCreateInfo;

typedef struct VkCommandBufferAllocateInfo {
	VkStructureType sType;
	const void *pNext;
	VkCommandPool commandPool;
	VkCommandBufferLevel level;
	uint32_t commandBufferCount;
} VkCommandBufferAllocateInfo;

typedef struct VkCommandBufferBeginInfo {
	VkStructureType sType;
	const void *pNext;
	VkCommandBufferUsageFlags flags;
	const VkCommandBufferInheritanceInfo *pInheritanceInfo;
} VkCommandBufferBeginInfo;

typedef struct VkSubmitInfo {
	VkStructureType sType;
	const void *pNext;
	uint32_t waitSemaphoreCount;
	const VkSemaphore *pWaitSemaphores;
	const VkPipelineStageFlags *pWaitDstStageMask;
	uint32_t commandBufferCount;
	const VkCommandBuffer *pCommandBuffers;
	uint32_t signalSemaphoreCount;
	const VkSemaphore *pSignalSemaphores;
} VkSubmitInfo;

typedef struct VkFenceCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkFenceCreateFlags flags;
} VkFenceCreateInfo;

#define VK_MAX_EXTENSION_NAME_SIZE 256
#define VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT 0x00000080

typedef uint32_t VkExternalMemoryHandleTypeFlagBits;
typedef uint32_t VkExternalMemoryHandleTypeFlags;

typedef struct VkExtensionProperties {
	char extensionName[VK_MAX_EXTENSION_NAME_SIZE];
	uint32_t specVersion;
} VkExtensionProperties;

typedef struct VkPhysicalDeviceProperties2 {
	VkStructureType sType;
	void *pNext;
	VkPhysicalDeviceProperties properties;
} VkPhysicalDeviceProperties2;

typedef struct VkPhysicalDeviceExternalMemoryHostPropertiesEXT {
	VkStructureType sType;
	void *pNext;
	VkDeviceSize minImportedHostPointerAlignment;
} VkPhysicalDeviceExternalMemoryHostPropertiesEXT;

typedef struct VkExternalMemoryBufferCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkExternalMemoryHandleTypeFlags handleTypes;
} VkExternalMemoryBufferCreateInfo;

typedef struct VkImportMemoryHostPointerInfoEXT {
	VkStructureType sType;
	const void *pNext;
	VkExternalMemoryHandleTypeFlagBits handleType;
	void *pHostPointer;
} VkImportMemoryHostPointerInfoEXT;

typedef struct VkMemoryHostPointerPropertiesEXT {
	VkStructureType sType;
	void *pNext;
	uint32_t memoryTypeBits;
} VkMemoryHostPointerPropertiesEXT;

typedef VkResult(VKAPI_PTR *PFN_vkCreateInstance)(
		const VkInstanceCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkInstance *pInstance);
typedef VkResult(VKAPI_PTR *PFN_vkEnumeratePhysicalDevices)(
		VkInstance instance, uint32_t *pPhysicalDeviceCount,
		VkPhysicalDevice *pPhysicalDevices);
typedef void(VKAPI_PTR *PFN_vkGetPhysicalDeviceProperties)(
		VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties *pProperties);
typedef void(VKAPI_PTR *PFN_vkGetPhysicalDeviceFeatures)(
		VkPhysicalDevice physicalDevice, VkPhysicalDeviceFeatures *pFeatures);
typedef void(VKAPI_PTR *PFN_vkGetPhysicalDeviceQueueFamilyProperties)(
		VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount,
		VkQueueFamilyProperties *pQueueFamilyProperties);
typedef void(VKAPI_PTR *PFN_vkGetPhysicalDeviceMemoryProperties)(
		VkPhysicalDevice physicalDevice,
		VkPhysicalDeviceMemoryProperties *pMemoryProperties);
typedef void(VKAPI_PTR *PFN_vkGetPhysicalDeviceProperties2)(
		VkPhysicalDevice physicalDevice,
		VkPhysicalDeviceProperties2 *pProperties);
typedef VkResult(VKAPI_PTR *PFN_vkEnumerateDeviceExtensionProperties)(
		VkPhysicalDevice physicalDevice, const char *pLayerName,
		uint32_t *pPropertyCount, VkExtensionProperties *pProperties);
typedef void(VKAPI_PTR *PFN_vkVoidFunction_mcc)(void);
typedef PFN_vkVoidFunction_mcc(VKAPI_PTR *PFN_vkGetDeviceProcAddr)(
		VkDevice device, const char *pName);
typedef VkResult(VKAPI_PTR *PFN_vkGetMemoryHostPointerPropertiesEXT)(
		VkDevice device, VkExternalMemoryHandleTypeFlagBits handleType,
		const void *pHostPointer,
		VkMemoryHostPointerPropertiesEXT *pMemoryHostPointerProperties);
typedef VkResult(VKAPI_PTR *PFN_vkCreateDevice)(
		VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);
typedef void(VKAPI_PTR *PFN_vkDestroyDevice)(
		VkDevice device, const VkAllocationCallbacks *pAllocator);
typedef void(VKAPI_PTR *PFN_vkDestroyInstance)(
		VkInstance instance, const VkAllocationCallbacks *pAllocator);
typedef void(VKAPI_PTR *PFN_vkGetDeviceQueue)(VkDevice device,
																							uint32_t queueFamilyIndex,
																							uint32_t queueIndex,
																							VkQueue *pQueue);
typedef VkResult(VKAPI_PTR *PFN_vkDeviceWaitIdle)(VkDevice device);
typedef VkResult(VKAPI_PTR *PFN_vkCreateBuffer)(
		VkDevice device, const VkBufferCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkBuffer *pBuffer);
typedef void(VKAPI_PTR *PFN_vkDestroyBuffer)(
		VkDevice device, VkBuffer buffer, const VkAllocationCallbacks *pAllocator);
typedef void(VKAPI_PTR *PFN_vkGetBufferMemoryRequirements)(
		VkDevice device, VkBuffer buffer, VkMemoryRequirements *pMemoryRequirements);
typedef VkResult(VKAPI_PTR *PFN_vkAllocateMemory)(
		VkDevice device, const VkMemoryAllocateInfo *pAllocateInfo,
		const VkAllocationCallbacks *pAllocator, VkDeviceMemory *pMemory);
typedef void(VKAPI_PTR *PFN_vkFreeMemory)(
		VkDevice device, VkDeviceMemory memory,
		const VkAllocationCallbacks *pAllocator);
typedef VkResult(VKAPI_PTR *PFN_vkBindBufferMemory)(VkDevice device,
																										VkBuffer buffer,
																										VkDeviceMemory memory,
																										VkDeviceSize memoryOffset);
typedef VkResult(VKAPI_PTR *PFN_vkMapMemory)(VkDevice device,
																						 VkDeviceMemory memory,
																						 VkDeviceSize offset,
																						 VkDeviceSize size,
																						 VkMemoryMapFlags flags,
																						 void **ppData);
typedef void(VKAPI_PTR *PFN_vkUnmapMemory)(VkDevice device,
																					 VkDeviceMemory memory);
typedef VkResult(VKAPI_PTR *PFN_vkCreateDescriptorSetLayout)(
		VkDevice device, const VkDescriptorSetLayoutCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkDescriptorSetLayout *pSetLayout);
typedef void(VKAPI_PTR *PFN_vkDestroyDescriptorSetLayout)(
		VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
		const VkAllocationCallbacks *pAllocator);
typedef VkResult(VKAPI_PTR *PFN_vkCreateDescriptorPool)(
		VkDevice device, const VkDescriptorPoolCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkDescriptorPool *pDescriptorPool);
typedef void(VKAPI_PTR *PFN_vkDestroyDescriptorPool)(
		VkDevice device, VkDescriptorPool descriptorPool,
		const VkAllocationCallbacks *pAllocator);
typedef VkResult(VKAPI_PTR *PFN_vkAllocateDescriptorSets)(
		VkDevice device, const VkDescriptorSetAllocateInfo *pAllocateInfo,
		VkDescriptorSet *pDescriptorSets);
typedef void(VKAPI_PTR *PFN_vkUpdateDescriptorSets)(
		VkDevice device, uint32_t descriptorWriteCount,
		const VkWriteDescriptorSet *pDescriptorWrites, uint32_t descriptorCopyCount,
		const VkCopyDescriptorSet *pDescriptorCopies);
typedef VkResult(VKAPI_PTR *PFN_vkCreateShaderModule)(
		VkDevice device, const VkShaderModuleCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkShaderModule *pShaderModule);
typedef void(VKAPI_PTR *PFN_vkDestroyShaderModule)(
		VkDevice device, VkShaderModule shaderModule,
		const VkAllocationCallbacks *pAllocator);
typedef VkResult(VKAPI_PTR *PFN_vkCreatePipelineLayout)(
		VkDevice device, const VkPipelineLayoutCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkPipelineLayout *pPipelineLayout);
typedef void(VKAPI_PTR *PFN_vkDestroyPipelineLayout)(
		VkDevice device, VkPipelineLayout pipelineLayout,
		const VkAllocationCallbacks *pAllocator);
typedef VkResult(VKAPI_PTR *PFN_vkCreateComputePipelines)(
		VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
		const VkComputePipelineCreateInfo *pCreateInfos,
		const VkAllocationCallbacks *pAllocator, VkPipeline *pPipelines);
typedef void(VKAPI_PTR *PFN_vkDestroyPipeline)(
		VkDevice device, VkPipeline pipeline,
		const VkAllocationCallbacks *pAllocator);
typedef VkResult(VKAPI_PTR *PFN_vkCreateCommandPool)(
		VkDevice device, const VkCommandPoolCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkCommandPool *pCommandPool);
typedef void(VKAPI_PTR *PFN_vkDestroyCommandPool)(
		VkDevice device, VkCommandPool commandPool,
		const VkAllocationCallbacks *pAllocator);
typedef VkResult(VKAPI_PTR *PFN_vkAllocateCommandBuffers)(
		VkDevice device, const VkCommandBufferAllocateInfo *pAllocateInfo,
		VkCommandBuffer *pCommandBuffers);
typedef VkResult(VKAPI_PTR *PFN_vkBeginCommandBuffer)(
		VkCommandBuffer commandBuffer, const VkCommandBufferBeginInfo *pBeginInfo);
typedef VkResult(VKAPI_PTR *PFN_vkEndCommandBuffer)(
		VkCommandBuffer commandBuffer);
typedef void(VKAPI_PTR *PFN_vkCmdBindPipeline)(
		VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
		VkPipeline pipeline);
typedef void(VKAPI_PTR *PFN_vkCmdBindDescriptorSets)(
		VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
		VkPipelineLayout layout, uint32_t firstSet, uint32_t descriptorSetCount,
		const VkDescriptorSet *pDescriptorSets, uint32_t dynamicOffsetCount,
		const uint32_t *pDynamicOffsets);
typedef void(VKAPI_PTR *PFN_vkCmdDispatch)(VkCommandBuffer commandBuffer,
																					 uint32_t groupCountX,
																					 uint32_t groupCountY,
																					 uint32_t groupCountZ);
typedef VkResult(VKAPI_PTR *PFN_vkQueueSubmit)(VkQueue queue,
																							 uint32_t submitCount,
																							 const VkSubmitInfo *pSubmits,
																							 VkFence fence);
typedef VkResult(VKAPI_PTR *PFN_vkCreateFence)(
		VkDevice device, const VkFenceCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkFence *pFence);
typedef void(VKAPI_PTR *PFN_vkDestroyFence)(
		VkDevice device, VkFence fence, const VkAllocationCallbacks *pAllocator);
typedef struct VkPipelineCacheCreateInfo {
	VkStructureType sType;
	const void *pNext;
	VkFlags flags;
	size_t initialDataSize;
	const void *pInitialData;
} VkPipelineCacheCreateInfo;

typedef VkResult(VKAPI_PTR *PFN_vkCreatePipelineCache)(
		VkDevice device, const VkPipelineCacheCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkPipelineCache *pPipelineCache);
typedef void(VKAPI_PTR *PFN_vkDestroyPipelineCache)(
		VkDevice device, VkPipelineCache pipelineCache,
		const VkAllocationCallbacks *pAllocator);
typedef VkResult(VKAPI_PTR *PFN_vkResetFences)(VkDevice device,
																							 uint32_t fenceCount,
																							 const VkFence *pFences);
typedef VkResult(VKAPI_PTR *PFN_vkResetCommandBuffer)(
		VkCommandBuffer commandBuffer, VkFlags flags);
typedef VkResult(VKAPI_PTR *PFN_vkWaitForFences)(VkDevice device,
																								 uint32_t fenceCount,
																								 const VkFence *pFences,
																								 VkBool32 waitAll,
																								 uint64_t timeout);

#define MCC_VK_FNS(X)                                                          \
	X(vkCreateInstance)                                                          \
	X(vkEnumeratePhysicalDevices)                                                \
	X(vkGetPhysicalDeviceProperties)                                             \
	X(vkGetPhysicalDeviceFeatures)                                               \
	X(vkGetPhysicalDeviceQueueFamilyProperties)                                  \
	X(vkGetPhysicalDeviceMemoryProperties)                                       \
	X(vkCreateDevice)                                                            \
	X(vkGetDeviceQueue)                                                          \
	X(vkDeviceWaitIdle)                                                          \
	X(vkCreateBuffer)                                                            \
	X(vkDestroyBuffer)                                                           \
	X(vkGetBufferMemoryRequirements)                                             \
	X(vkAllocateMemory)                                                          \
	X(vkFreeMemory)                                                              \
	X(vkBindBufferMemory)                                                        \
	X(vkMapMemory)                                                               \
	X(vkUnmapMemory)                                                             \
	X(vkCreateDescriptorSetLayout)                                               \
	X(vkDestroyDescriptorSetLayout)                                              \
	X(vkCreateDescriptorPool)                                                    \
	X(vkDestroyDescriptorPool)                                                   \
	X(vkAllocateDescriptorSets)                                                  \
	X(vkUpdateDescriptorSets)                                                    \
	X(vkCreateShaderModule)                                                      \
	X(vkDestroyShaderModule)                                                     \
	X(vkCreatePipelineLayout)                                                    \
	X(vkDestroyPipelineLayout)                                                   \
	X(vkCreateComputePipelines)                                                  \
	X(vkDestroyPipeline)                                                         \
	X(vkCreateCommandPool)                                                       \
	X(vkDestroyCommandPool)                                                      \
	X(vkAllocateCommandBuffers)                                                  \
	X(vkBeginCommandBuffer)                                                      \
	X(vkEndCommandBuffer)                                                        \
	X(vkCmdBindPipeline)                                                         \
	X(vkCmdBindDescriptorSets)                                                   \
	X(vkCmdDispatch)                                                             \
	X(vkQueueSubmit)                                                             \
	X(vkCreateFence)                                                             \
	X(vkDestroyFence)                                                            \
	X(vkWaitForFences)                                                           \
	X(vkCreatePipelineCache)                                                     \
	X(vkDestroyPipelineCache)                                                    \
	X(vkResetFences)                                                             \
	X(vkResetCommandBuffer)

/* vkDestroyDevice and vkDestroyInstance are core 1.0 and every loader has
 * them, but MCC_VK_FNS is a hard requirement -- one missing symbol there
 * disables the device outright. A loader that somehow lacks a destructor
 * should cost us a teardown, not the GPU, so bind them softly and null-check
 * at the one site that calls them. */
#define MCC_VK_OPT_FNS(X)                                                      \
	X(vkGetPhysicalDeviceProperties2)                                            \
	X(vkEnumerateDeviceExtensionProperties)                                      \
	X(vkGetDeviceProcAddr)                                                       \
	X(vkDestroyDevice)                                                           \
	X(vkDestroyInstance)

#define MCC_VK_DECL(n) static PFN_##n n;
MCC_VK_FNS(MCC_VK_DECL)
MCC_VK_OPT_FNS(MCC_VK_DECL)
#undef MCC_VK_DECL

static PFN_vkGetMemoryHostPointerPropertiesEXT vkGetMemoryHostPointerPropertiesEXT;

static int mcc_vk_tried;
static int mcc_vk_ok;

static const char *const mcc_fe_sonames[] = {
#if MCC_HOST_WIN32
		"ucrtbase.dll", "msvcrt.dll",
#elif MCC_HOST_DARWIN
		"libSystem.B.dylib",
#else
		"libm.so.6", "libm.so",
#endif
		NULL};

static int mcc_fe_bind(void) {
	int i;
	mcc_fe_get = (MccFeGetFn)host_dlsym_process("fegetenv");
	mcc_fe_set = (MccFeSetFn)host_dlsym_process("fesetenv");
	for (i = 0; (!mcc_fe_get || !mcc_fe_set) && mcc_fe_sonames[i]; i++) {
		void *h = host_dlopen(mcc_fe_sonames[i]);
		if (!h)
			continue;
		mcc_fe_get = (MccFeGetFn)host_dlsym(h, "fegetenv");
		mcc_fe_set = (MccFeSetFn)host_dlsym(h, "fesetenv");
	}
	if (mcc_fe_get && mcc_fe_set)
		return 1;
	if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
		fprintf(stderr, "[ladder-gpu] no fegetenv/fesetenv in the process\n");
	return 0;
}

/* Darwin has no Vulkan driver; MoltenVK implements the API over Metal and is
 * what MCC_GPU_BACKEND=vulkan runs on there.  The Vulkan SDK installs a
 * libvulkan.dylib loader, but Homebrew's molten-vk ships only
 * libMoltenVK.dylib, which exports the same entry points -- so try both, and
 * the two Homebrew prefixes by absolute path because dlopen does not search
 * them.  MCC_VULKAN_LIB overrides all of it. */
static const char *const mcc_vk_sonames[] = {
#if MCC_HOST_WIN32
		"vulkan-1.dll",
#elif MCC_HOST_DARWIN
		"libvulkan.dylib", "libvulkan.1.dylib", "libMoltenVK.dylib",
		"/opt/homebrew/lib/libMoltenVK.dylib", "/usr/local/lib/libMoltenVK.dylib",
#else
		"libvulkan.so.1", "libvulkan.so",
#endif
		NULL};

static int mcc_vk_load(void) {
	void *h = NULL;
	const char *over;
	int i;

	if (mcc_vk_tried)
		return mcc_vk_ok;
	mcc_vk_tried = 1;
	over = getenv("MCC_VULKAN_LIB");
	if (over && *over)
		h = host_dlopen(over);
	else
		for (i = 0; !h && mcc_vk_sonames[i]; i++)
			h = host_dlopen(mcc_vk_sonames[i]);
	if (!h) {
		if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
			fprintf(stderr, "[ladder-gpu] no vulkan loader (%s)\n", host_dlerror());
		return 0;
	}
#define MCC_VK_BIND(n)                                                         \
	n = (PFN_##n)host_dlsym(h, #n);                                              \
	if (!n) {                                                                    \
		if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))                                \
			fprintf(stderr, "[ladder-gpu] missing symbol %s\n", #n);                 \
		return 0;                                                                  \
	}
	MCC_VK_FNS(MCC_VK_BIND)
#undef MCC_VK_BIND
#define MCC_VK_BINDOPT(n) n = (PFN_##n)host_dlsym(h, #n);
	MCC_VK_OPT_FNS(MCC_VK_BINDOPT)
#undef MCC_VK_BINDOPT
	if (!mcc_fe_bind())
		return 0;
	mcc_vk_ok = 1;
	return 1;
}

typedef struct MccGpu {
	int tried;
	int ok;
	int lost;
	VkPhysicalDevice phys;
	VkDevice dev;
	VkQueue q;
	unsigned qfam;
	char name[256];
	int f64;
	int hostimp;
	unsigned long hostimp_align;
	unsigned long maxsbrange;
	char hostimp_why[192];
} MccGpu;

/* Multi-GPU (T-win-50022): mcc holds every device that clears the capability
 * floor and routes each dispatch to one of them. The VkInstance and the
 * aggregate dispatch counters are shared across devices; the logical device,
 * queue, and resident context are one MccGpu + one mcc_vkr slot per held
 * device, indexed by the routed device `mcc_gpu_cur`. The `mcc_gpu`/`mcc_vkr`
 * macros resolve to the routed slot so the ~300 existing `.`-field accesses on
 * the dispatch path read unchanged; only the instance, the counters, the init
 * lifecycle, teardown, and the router are aware of the array. */
#ifndef MCC_GPU_MAXDEV
#define MCC_GPU_MAXDEV 8
#endif
static VkInstance mcc_gpu_inst;
static int mcc_gpu_inst_tried;
static int mcc_gpu_ndev;
static int mcc_gpu_cur;
static MccGpu mcc_gpu_arr[MCC_GPU_MAXDEV];
#define mcc_gpu (mcc_gpu_arr[mcc_gpu_cur])

static int mcc_vk_diag(void) {
	return getenv("MCC_AST_EVAL_LADDER_GPU_DIAG") != 0;
}

/* The fence timeout was a hardcoded 30 s, which made the pending-command-buffer
 * path unreachable by any test. As a named tunable one cell can force a timeout
 * on a real device with no fault injection and no hang. */
static uint64_t mcc_vk_fence_ns(void) {
	const char *e = getenv("MCC_GPU_FENCE_NS");
	if (e && e[0]) {
		char *end = 0;
		unsigned long long v = strtoull(e, &end, 10);
		if (end != e && v > 0)
			return (uint64_t)v;
	}
	return 30ULL * 1000000000ULL;
}

/* --- L2'(ii): device loss ------------------------------------------------ *
 * VK_ERROR_DEVICE_LOST was declared here and compared against nowhere. It is
 * not one failure among many: it is the one after which every handle derived
 * from the device is unusable and every subsequent call has undefined
 * behaviour, so a return code that was only ever turned into `return 0` left
 * mcc_gpu.ok set and the next dispatch walked straight back into the driver.
 *
 * The spec lists it for a small, fixed set of calls -- queue submission,
 * fence and idle waits, and memory mapping -- so rather than sprinkling
 * comparisons, every one of those goes through this function and nothing
 * else needs to know the code exists. It returns its argument so it wraps a
 * call in place.
 *
 * MCC_GPU_FORCE_DEVICE_LOST names one of those call sites (or "*" for all) and
 * makes it report the loss. Device loss cannot be provoked on a healthy
 * device, and the path that matters is precisely the one that only runs when
 * the device is gone; this is the same trick MCC_GPU_FENCE_NS plays on the
 * fence timeout, read at the point of use for the same reason -- a cell has to
 * be able to arm it after the device it means to lose has come up. */
static VkResult mcc_vk_chk(VkResult r, const char *what) {
	const char *lose = getenv("MCC_GPU_FORCE_DEVICE_LOST");
	if (lose && lose[0] && (!strcmp(lose, what) || !strcmp(lose, "*")))
		r = VK_ERROR_DEVICE_LOST;
	if (r == VK_ERROR_DEVICE_LOST) {
		if (!mcc_gpu.lost && mcc_vk_diag())
			fprintf(stderr,
							"[gpu-vk] %s reported VK_ERROR_DEVICE_LOST; the device is dead, "
							"no further dispatch will be attempted and the resident objects "
							"are stranded\n",
							what);
		mcc_gpu.lost = 1;
		mcc_gpu.ok = 0;
	}
	return r;
}

#define MCC_VK_EXT_HOSTMEM "VK_EXT_external_memory_host"

static const char *const mcc_vk_hostmem_ext = MCC_VK_EXT_HOSTMEM;

static void mcc_vk_hostimp_no(const char *why) {
	mcc_gpu.hostimp = 0;
	mcc_gpu.hostimp_align = 0;
	snprintf(mcc_gpu.hostimp_why, sizeof mcc_gpu.hostimp_why, "%s", why);
}

static int mcc_vk_want_hostimport(void) {
	VkExtensionProperties *ep;
	uint32_t n = 0, i;
	int found = 0;
	VkResult r;

	if (getenv("MCC_GPU_NO_HOST_IMPORT")) {
		mcc_vk_hostimp_no("host-pointer import disabled by MCC_GPU_NO_HOST_IMPORT");
		return 0;
	}
	if (!vkEnumerateDeviceExtensionProperties || !vkGetPhysicalDeviceProperties2 ||
			!vkGetDeviceProcAddr) {
		mcc_vk_hostimp_no("the Vulkan loader does not export "
											"vkEnumerateDeviceExtensionProperties, "
											"vkGetPhysicalDeviceProperties2 and vkGetDeviceProcAddr");
		return 0;
	}
	{
		VkPhysicalDeviceProperties pp;
		memset(&pp, 0, sizeof pp);
		vkGetPhysicalDeviceProperties(mcc_gpu.phys, &pp);
		if (pp.apiVersion < VK_API_VERSION_1_1) {
			mcc_vk_hostimp_no("the device is below Vulkan 1.1, so "
												"vkGetPhysicalDeviceProperties2 cannot be called to read "
												"minImportedHostPointerAlignment");
			return 0;
		}
	}
	r = vkEnumerateDeviceExtensionProperties(mcc_gpu.phys, 0, &n, 0);
	if (r != VK_SUCCESS || !n) {
		mcc_vk_hostimp_no("the device enumerates no extensions");
		return 0;
	}
	ep = (VkExtensionProperties *)MCC_GPU_MALLOC((size_t)n * sizeof *ep);
	if (!ep) {
		mcc_vk_hostimp_no("out of memory enumerating device extensions");
		return 0;
	}
	memset(ep, 0, (size_t)n * sizeof *ep);
	r = vkEnumerateDeviceExtensionProperties(mcc_gpu.phys, 0, &n, ep);
	if (r == VK_SUCCESS || r == VK_INCOMPLETE)
		for (i = 0; i < n && !found; i++)
			if (!strcmp(ep[i].extensionName, MCC_VK_EXT_HOSTMEM))
				found = 1;
	MCC_GPU_FREE(ep);
	if (!found) {
		mcc_vk_hostimp_no("the device does not support " MCC_VK_EXT_HOSTMEM);
		return 0;
	}
	{
		VkPhysicalDeviceExternalMemoryHostPropertiesEXT hp;
		VkPhysicalDeviceProperties2 p2;
		memset(&hp, 0, sizeof hp);
		hp.sType =
				VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
		memset(&p2, 0, sizeof p2);
		p2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		p2.pNext = &hp;
		vkGetPhysicalDeviceProperties2(mcc_gpu.phys, &p2);
		if (!hp.minImportedHostPointerAlignment ||
				(hp.minImportedHostPointerAlignment &
				 (hp.minImportedHostPointerAlignment - 1))) {
			mcc_vk_hostimp_no("the device reports a minImportedHostPointerAlignment "
												"that is not a power of two");
			return 0;
		}
		mcc_gpu.hostimp_align = (unsigned long)hp.minImportedHostPointerAlignment;
	}
	mcc_gpu.hostimp = 1;
	mcc_gpu.hostimp_why[0] = 0;
	return 1;
}

static int mcc_vk_compute_qfam(VkPhysicalDevice d, unsigned *out) {
	VkQueueFamilyProperties qf[32];
	unsigned nq = 32, i;
	vkGetPhysicalDeviceQueueFamilyProperties(d, &nq, qf);
	if (nq > 32)
		nq = 32;
	for (i = 0; i < nq; i++)
		if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			*out = i;
			return 1;
		}
	return 0;
}

#define MCC_VK_MIN_SB_RANGE (1u << 20)
#define MCC_VK_MIN_GROUPS ((1u << 20) / MCC_GPU_LOCAL_SIZE)

static long mcc_vk_device_score(const VkPhysicalDeviceProperties *p, int f64) {
	long s;

	if (p->limits.maxComputeWorkGroupInvocations < MCC_GPU_LOCAL_SIZE ||
			p->limits.maxComputeWorkGroupSize[0] < MCC_GPU_LOCAL_SIZE ||
			p->limits.maxComputeWorkGroupCount[0] < MCC_VK_MIN_GROUPS ||
			p->limits.maxBoundDescriptorSets < 1 ||
			p->limits.maxPerStageDescriptorStorageBuffers < 3 ||
			p->limits.maxStorageBufferRange < MCC_VK_MIN_SB_RANGE)
		return -1;

	switch (p->deviceType) {
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		s = 5000;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		s = 4000;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		s = 3000;
		break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		s = 1000;
		break;
	default:
		s = 2000;
		break;
	}
	if (f64)
		s += 400;
	if (p->apiVersion >= VK_API_VERSION_1_1)
		s += 100;
	{
		unsigned long mb = (unsigned long)(p->limits.maxStorageBufferRange >> 20);
		if (mb > 64)
			mb = 64;
		s += (long)mb;
	}
	return s;
}

static int mcc_vk_pin_matches(const char *pin, unsigned idx,
															const VkPhysicalDeviceProperties *p) {
	const char *q;
	size_t i, j, n;
	int isnum = 1;
	for (q = pin; *q; q++)
		if (*q < '0' || *q > '9')
			isnum = 0;
	if (isnum && pin[0])
		return (unsigned)atoi(pin) == idx;
	n = strlen(pin);
	if (!n)
		return 0;
	for (i = 0; p->deviceName[i]; i++) {
		for (j = 0; j < n; j++) {
			char a = p->deviceName[i + j], b = pin[j];
			if (a >= 'A' && a <= 'Z')
				a = (char)(a - 'A' + 'a');
			if (b >= 'A' && b <= 'Z')
				b = (char)(b - 'A' + 'a');
			if (a != b)
				break;
		}
		if (j == n)
			return 1;
	}
	return 0;
}

/* Create the logical device + one compute queue for the currently-routed slot
   (mcc_gpu.phys / mcc_gpu.qfam already filled), returning 1 on success. Extracted
   from mcc_gpu_init for T-win-50022 slice 1b so init can call it once per held
   device. Operates entirely on the routed slot (mcc_gpu = mcc_gpu_arr[mcc_gpu_cur]);
   mcc_vk_want_hostimport() reads mcc_gpu.phys, so host-import is decided per device.
   The device-specific vkGetMemoryHostPointerPropertiesEXT it resolves is kept
   consistent with the routed device by re-resolving in mcc_gpu_route(). */
static int mcc_vk_create_device_slot(void) {
	VkDeviceQueueCreateInfo qci;
	VkDeviceCreateInfo dci;
	VkPhysicalDeviceFeatures feat;
	float prio = 1.0f;

	memset(&qci, 0, sizeof qci);
	qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qci.queueFamilyIndex = mcc_gpu.qfam;
	qci.queueCount = 1;
	qci.pQueuePriorities = &prio;
	memset(&dci, 0, sizeof dci);
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	memset(&feat, 0, sizeof feat);
	{
		VkPhysicalDeviceFeatures have;
		memset(&have, 0, sizeof have);
		vkGetPhysicalDeviceFeatures(mcc_gpu.phys, &have);
		mcc_gpu.f64 = have.shaderFloat64 ? 1 : 0;
		feat.shaderFloat64 = have.shaderFloat64;
	}
	dci.pEnabledFeatures = &feat;
	if (mcc_vk_want_hostimport()) {
		dci.enabledExtensionCount = 1;
		dci.ppEnabledExtensionNames = &mcc_vk_hostmem_ext;
	}
	{
		VkResult _r = mcc_vk_chk(vkCreateDevice(mcc_gpu.phys, &dci, 0, &mcc_gpu.dev),
														 "vkCreateDevice");
		if (_r != VK_SUCCESS && mcc_gpu.hostimp) {
			mcc_vk_hostimp_no("vkCreateDevice refused " MCC_VK_EXT_HOSTMEM);
			dci.enabledExtensionCount = 0;
			dci.ppEnabledExtensionNames = 0;
			_r = mcc_vk_chk(vkCreateDevice(mcc_gpu.phys, &dci, 0, &mcc_gpu.dev),
											"vkCreateDevice");
		}
		if (_r != VK_SUCCESS) {
			if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
				fprintf(stderr, "[ladder-gpu] vkCreateDevice rc=%d\n", (int)_r);
			return 0;
		}
	}
	vkGetDeviceQueue(mcc_gpu.dev, mcc_gpu.qfam, 0, &mcc_gpu.q);
	if (mcc_gpu.hostimp) {
		vkGetMemoryHostPointerPropertiesEXT =
				(PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetDeviceProcAddr(
						mcc_gpu.dev, "vkGetMemoryHostPointerPropertiesEXT");
		if (!vkGetMemoryHostPointerPropertiesEXT)
			mcc_vk_hostimp_no("vkGetDeviceProcAddr did not resolve "
												"vkGetMemoryHostPointerPropertiesEXT");
	}
	if (mcc_vk_diag())
		fprintf(stderr, "[gpu-vk] host-pointer import %s align=%lu%s%s\n",
						mcc_gpu.hostimp ? "yes" : "no", mcc_gpu.hostimp_align,
						mcc_gpu.hostimp ? "" : " -- ", mcc_gpu.hostimp_why);
	mcc_gpu.lost = 0;
	mcc_gpu.ok = 1;
	return 1;
}

static int mcc_gpu_init(void) {
	VkApplicationInfo ai;
	VkInstanceCreateInfo ici;
	VkPhysicalDevice devs[8];
	/* One candidate per capability-passing device, best-first after the sort. */
	struct MccVkCand {
		VkPhysicalDevice phys;
		unsigned qfam;
		long score;
		char name[256];
		unsigned long maxsb;
	} cand[MCC_GPU_MAXDEV];
	int ncand = 0, ci, cj, made = 0;
	unsigned ndev = 0, i;

	if (mcc_gpu_closing)
		return 0;
	/* Gate on slot 0 explicitly, NOT via the mcc_gpu macro: an already-initialised
	 * call must return WITHOUT touching mcc_gpu_cur, or it resets the routed device
	 * (mcc_gpu_route) back to slot 0 every time the dispatch path re-checks init. cur
	 * is reset to 0 only on the real first init below. */
	if (mcc_gpu_arr[0].tried)
		return mcc_gpu_arr[0].ok;
	mcc_gpu_cur = 0;
	mcc_gpu.tried = 1;
	if (!mcc_vk_load())
		return 0;
	memset(&ai, 0, sizeof ai);
	ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	ai.pApplicationName = "mcc";
	ai.apiVersion = VK_API_VERSION_1_1;
	memset(&ici, 0, sizeof ici);
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.pApplicationInfo = &ai;
	{
		VkResult _r = vkCreateInstance(&ici, 0, &mcc_gpu_inst);
		if (_r != VK_SUCCESS) {
			if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
				fprintf(stderr, "[ladder-gpu] vkCreateInstance rc=%d\n", (int)_r);
			return 0;
		}
	}
	{
		/* Count first, then fill.  The count query must succeed outright; a loader
		 * that answers VK_INCOMPLETE to it is not telling us anything usable, and
		 * the handles such a loader writes are not valid (AMD's
		 * VK_LAYER_AMD_switchable_graphics does exactly this).  VK_INCOMPLETE from
		 * the *fill* is expected and fine: it only means there are more devices
		 * than devs[] holds and ndev is the number actually written, which is the
		 * set the scoring below ranks. */
		unsigned cap = (unsigned)(sizeof devs / sizeof devs[0]);
		VkResult _r = vkEnumeratePhysicalDevices(mcc_gpu_inst, &ndev, 0);
		if (_r != VK_SUCCESS || !ndev) {
			if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
				fprintf(stderr,
								"[ladder-gpu] vkEnumeratePhysicalDevices(count) rc=%d ndev=%u\n",
								(int)_r, ndev);
			return 0;
		}
		if (ndev > cap)
			ndev = cap;
		_r = vkEnumeratePhysicalDevices(mcc_gpu_inst, &ndev, devs);
		if ((_r != VK_SUCCESS && _r != VK_INCOMPLETE) || !ndev) {
			if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
				fprintf(stderr, "[ladder-gpu] vkEnumeratePhysicalDevices rc=%d ndev=%u\n",
								(int)_r, ndev);
			return 0;
		}
	}
	{
		const char *pin = getenv("MCC_GPU_DEVICE");
		int pinned = 0;
		for (i = 0; i < ndev && ncand < MCC_GPU_MAXDEV; i++) {
			VkPhysicalDeviceProperties cp;
			VkPhysicalDeviceFeatures cf;
			unsigned q = 0;
			long sc;
			memset(&cp, 0, sizeof cp);
			memset(&cf, 0, sizeof cf);
			vkGetPhysicalDeviceProperties(devs[i], &cp);
			if (pin && pin[0]) {
				if (!mcc_vk_pin_matches(pin, i, &cp))
					continue;
				pinned = 1;
			}
			if (!mcc_vk_compute_qfam(devs[i], &q)) {
				if (mcc_vk_diag())
					fprintf(stderr, "[gpu-vk] device %u \"%s\" has no compute queue\n", i,
									cp.deviceName);
				continue;
			}
			vkGetPhysicalDeviceFeatures(devs[i], &cf);
			sc = mcc_vk_device_score(&cp, cf.shaderFloat64 ? 1 : 0);
			if (mcc_vk_diag())
				fprintf(stderr, "[gpu-vk] device %u \"%s\" type=%d f64=%d score=%ld\n", i,
								cp.deviceName, (int)cp.deviceType, cf.shaderFloat64 ? 1 : 0, sc);
			if (sc < 0)
				continue;
			/* Slice 1b: collect EVERY capability-passing device, not just the best. */
			cand[ncand].phys = devs[i];
			cand[ncand].qfam = q;
			cand[ncand].score = sc;
			snprintf(cand[ncand].name, sizeof cand[ncand].name, "%s", cp.deviceName);
			cand[ncand].maxsb = (unsigned long)cp.limits.maxStorageBufferRange;
			ncand++;
		}
		if (ncand == 0) {
			if (pin && pin[0] && !pinned)
				fprintf(stderr,
								"[gpu-vk] MCC_GPU_DEVICE=\"%s\" matched none of the %u "
								"enumerated devices\n",
								pin, ndev);
			else if (mcc_vk_diag())
				fprintf(stderr,
								"[gpu-vk] none of the %u enumerated devices can run a "
								"%d-wide compute group over three storage buffers\n",
								ndev, MCC_GPU_LOCAL_SIZE);
			if (mcc_gpu_inst && vkDestroyInstance) {
				vkDestroyInstance(mcc_gpu_inst, 0);
				mcc_gpu_inst = 0;
			}
			return 0;
		}
		/* best-first: descending selection sort, stable on ties (keeps enumeration
		   order), so slot 0 stays the single best device slice 1a chose. */
		for (ci = 0; ci < ncand; ci++)
			for (cj = ci + 1; cj < ncand; cj++)
				if (cand[cj].score > cand[ci].score) {
					struct MccVkCand t = cand[ci];
					cand[ci] = cand[cj];
					cand[cj] = t;
				}
	}
	/* Slice 1b: create a logical device + queue for each passing device, best-first,
	   into its own slot. A device that fails to create is skipped, not fatal, unless
	   none succeed. */
	for (ci = 0; ci < ncand; ci++) {
		mcc_gpu_cur = made;
		mcc_gpu.phys = cand[ci].phys;
		mcc_gpu.qfam = cand[ci].qfam;
		snprintf(mcc_gpu.name, sizeof mcc_gpu.name, "%s", cand[ci].name);
		mcc_gpu.maxsbrange = cand[ci].maxsb;
		/* T-lin-10393: --jit-gpu-budget caps the usable VRAM (storage-buffer range). */
		if (mcc_gpu_vram_budget_pct >= 0)
			mcc_gpu.maxsbrange =
				(unsigned long)((double)cand[ci].maxsb * mcc_gpu_vram_budget_pct / 100.0);
		if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
			fprintf(stderr, "[ladder-gpu] init slot %d dev=%s qfam=%u score=%ld\n", made,
							mcc_gpu.name, mcc_gpu.qfam, cand[ci].score);
		if (mcc_vk_create_device_slot())
			made++;
	}
	mcc_gpu_cur = 0;
	if (made < 1) {
		if (mcc_gpu_inst && vkDestroyInstance) {
			vkDestroyInstance(mcc_gpu_inst, 0);
			mcc_gpu_inst = 0;
		}
		return 0;
	}
	/* mcc now HOLDS every capability-passing device, best-first in slots 0..made-1.
	 * Routing (mcc_gpu_route) selects among them; default cur==0 is the same single
	 * best device slice 1a held, so existing gpu/* cells are unchanged. The create
	 * loop left the device-specific host-import proc pointing at the last slot, so
	 * re-resolve it for the default routed slot 0. */
	/* T-lin-10393: --jit-gpu-devices caps how many of the held devices we expose.
	 * MCC_GPU_MAX_DEVICES is the env mirror the CLI-less device tools (slicerun)
	 * use to exercise the cap; an explicit CLI --jit-gpu-devices still wins. */
	if (mcc_gpu_max_devices < 1) {
		const char *e = getenv("MCC_GPU_MAX_DEVICES");
		if (e && atoi(e) >= 1)
			mcc_gpu_max_devices = atoi(e);
	}
	if (mcc_gpu_max_devices >= 1 && made > mcc_gpu_max_devices)
		made = mcc_gpu_max_devices;
	mcc_gpu_ndev = made;
	if (mcc_gpu.hostimp && vkGetDeviceProcAddr)
		vkGetMemoryHostPointerPropertiesEXT =
				(PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetDeviceProcAddr(
						mcc_gpu.dev, "vkGetMemoryHostPointerPropertiesEXT");
	return 1;
}

/* T-win-50022 slice 1b routing API — the CONTRACT lin's f64cross iterates instead
 * of the quiesce/reopen device cycle. */
int mcc_gpu_device_count(void) {
	if (!mcc_gpu_init())
		return 0;
	return mcc_gpu_ndev > 0 ? mcc_gpu_ndev : 0;
}

/* Route subsequent dispatches to held device `i` in [0, mcc_gpu_device_count()).
 * Returns 0 on success, -1 on a bad index. O(1): only the routed slot moves; each
 * slot's resident context (mcc_vkr) binds lazily on first dispatch. The
 * device-specific host-import proc is re-resolved so it matches the routed device. */
int mcc_gpu_route(int i) {
	if (i < 0 || i >= mcc_gpu_ndev)
		return -1;
	mcc_gpu_cur = i;
	if (mcc_gpu.hostimp && vkGetDeviceProcAddr)
		vkGetMemoryHostPointerPropertiesEXT =
				(PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetDeviceProcAddr(
						mcc_gpu.dev, "vkGetMemoryHostPointerPropertiesEXT");
	return 0;
}

/* Taking the *first* HOST_VISIBLE|HOST_COHERENT type is what the spec permits
 * and what the hardware punishes: on this host that is memoryTypes[2], plain
 * system RAM and not even HOST_CACHED, while memoryTypes[4] is
 * DEVICE_LOCAL|HOST_VISIBLE|HOST_COHERENT on the ReBAR heap. Under a B1-style
 * address space every interpreted load and store would be a PCIe transaction.
 * Score instead, preferring device-local (the kernel reads this far more often
 * than the host does) and then host-cached (so the readback is not an uncached
 * read), and keep first-match order as the tie-break so a device with one
 * qualifying type behaves exactly as before. */
static int mcc_gpu_mem_index(VkMemoryRequirements mr, uint32_t *out) {
	VkPhysicalDeviceMemoryProperties mp;
	unsigned i;
	int best = -1, best_score = -1;
	const char *force = getenv("MCC_GPU_MEMTYPE");
	unsigned want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
									VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	vkGetPhysicalDeviceMemoryProperties(mcc_gpu.phys, &mp);
	/* Overridable because the right answer is not obvious and is device-specific:
	 * DEVICE_LOCAL|HOST_VISIBLE (ReBAR) is fastest for the kernel and slowest for
	 * the host readback, which is an uncached read across PCIe. Which side
	 * dominates depends on lanes-per-dispatch, so it has to be measurable. */
	if (force && force[0]) {
		unsigned idx = (unsigned)strtoul(force, 0, 10);
		if (idx < mp.memoryTypeCount && (mr.memoryTypeBits & (1u << idx)) &&
				(mp.memoryTypes[idx].propertyFlags & want) == want) {
			if (mcc_vk_diag())
				fprintf(stderr, "[gpu-vk] memory type %u forced flags=0x%x\n", idx,
								(unsigned)mp.memoryTypes[idx].propertyFlags);
			*out = idx;
			return 1;
		}
	}
	for (i = 0; i < mp.memoryTypeCount; i++) {
		unsigned f = mp.memoryTypes[i].propertyFlags;
		int score;
		if (!(mr.memoryTypeBits & (1u << i)) || (f & want) != want)
			continue;
		/* HOST_CACHED dominates, and by a lot. Measured on this host at 256-node
		 * slices: type 3 (HOST_VISIBLE|COHERENT|CACHED) 13.4 ns/lane, type 2
		 * (HOST_VISIBLE|COHERENT) 54.2, type 4 (DEVICE_LOCAL|HOST_VISIBLE|
		 * COHERENT, i.e. ReBAR) 224.9 -- a 16.8x spread, and the ReBAR type that
		 * looks best on paper is the worst by far. The reason is which side of the
		 * bus does the most traffic: on the emitter path the kernel touches each
		 * live-in once and writes one result, while the host packs, uploads,
		 * downloads and unpacks every lane, so an uncached readback across PCIe is
		 * the whole cost. I2(D)'s argument for DEVICE_LOCAL is about the B1
		 * interpreter, where the kernel does the memory traffic instead -- so keep
		 * device-local as the tie-break, and revisit when that path exists. */
		score = 0;
		if (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
			score += 2;
		if (f & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			score += 1;
		if (score > best_score) {
			best_score = score;
			best = (int)i;
		}
	}
	if (best < 0)
		return 0;
	if (mcc_vk_diag())
		fprintf(stderr, "[gpu-vk] memory type %d flags=0x%x score=%d\n", best,
						(unsigned)mp.memoryTypes[best].propertyFlags, best_score);
	*out = (uint32_t)best;
	return 1;
}

static int mcc_gpu_buffer(VkDeviceSize size, VkBuffer *buf, VkDeviceMemory *mem,
													void **map) {
	VkBufferCreateInfo bci;
	VkMemoryRequirements mr;
	VkMemoryAllocateInfo mai;
	uint32_t idx;
	memset(&bci, 0, sizeof bci);
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(mcc_gpu.dev, &bci, 0, buf) != VK_SUCCESS)
		return 0;
	vkGetBufferMemoryRequirements(mcc_gpu.dev, *buf, &mr);
	if (!mcc_gpu_mem_index(mr, &idx)) {
		vkDestroyBuffer(mcc_gpu.dev, *buf, 0);
		return 0;
	}
	memset(&mai, 0, sizeof mai);
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = idx;
	if (vkAllocateMemory(mcc_gpu.dev, &mai, 0, mem) != VK_SUCCESS) {
		vkDestroyBuffer(mcc_gpu.dev, *buf, 0);
		return 0;
	}
	if (vkBindBufferMemory(mcc_gpu.dev, *buf, *mem, 0) != VK_SUCCESS ||
			mcc_vk_chk(vkMapMemory(mcc_gpu.dev, *mem, 0, size, 0, map),
								 "vkMapMemory") != VK_SUCCESS) {
		vkFreeMemory(mcc_gpu.dev, *mem, 0);
		vkDestroyBuffer(mcc_gpu.dev, *buf, 0);
		return 0;
	}
	return 1;
}

/* --- L3 residency ------------------------------------------------------- *
 * Everything below except the command recording and the submit is a pure
 * function of (code, nlive) or of nothing at all, and all of it used to be
 * created and destroyed on every single dispatch -- ten object classes, with
 * VK_NULL_HANDLE for the pipeline cache, so the driver recompiled the SPIR-V
 * every call. Measured cost of that on this host: ~640 us of fixed overhead per
 * dispatch, which is larger than any per-lane advantage the device has and is
 * why the first H6 table said no slice can ever break even.
 *
 * Nothing here is freed on the dispatch path. The only teardown is quiesce. */

#define MCC_VK_CACHE_MAX 64

typedef struct MccVkPipe {
	uint64_t key;
	VkShaderModule sm;
	VkPipelineLayout play;
	VkPipeline pipe;
} MccVkPipe;

static struct MccVkr {
	int ready;
	VkDescriptorSetLayout dsl;
	VkDescriptorPool dpool;
	VkDescriptorSet dset;
	VkCommandPool cpool;
	VkCommandBuffer cb;
	VkFence fence;
	VkPipelineCache pcache;
	VkBuffer bin, bout, bmem;
	VkDeviceMemory min_, mout, mmem;
	void *pin, *pout, *pmem;
	VkDeviceSize binsz, boutsz, bmemsz;
	int dsdirty;
	int memimported;
	/* Raised between a successful submit and the fence wait that proves the
	 * submit completed. It stays raised exactly when a command buffer is still
	 * referencing every object below, which is the one state in which the
	 * teardown must not touch them and must not wait for them forever. */
	int pending;
	MccVkPipe cache[MCC_VK_CACHE_MAX];
	int ncache, next;
} mcc_vkr_arr[MCC_GPU_MAXDEV];
#define mcc_vkr (mcc_vkr_arr[mcc_gpu_cur])

static uint64_t mcc_vk_key(const uint32_t *code, int nwords, int nlive) {
	uint64_t h = 1469598103934665603ull;
	int i;
	for (i = 0; i < nwords; i++) {
		h ^= code[i];
		h *= 1099511628211ull;
	}
	h ^= (uint64_t)nwords * 31 + (uint64_t)nlive;
	return h ? h : 1;
}

static int mcc_vk_resident(void) {
	VkDescriptorSetLayoutBinding dslb[3];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkDescriptorPoolSize dps;
	VkDescriptorPoolCreateInfo dpci;
	VkDescriptorSetAllocateInfo dsai;
	VkCommandPoolCreateInfo cpoolci;
	VkCommandBufferAllocateInfo cbai;
	VkFenceCreateInfo fci;
	VkPipelineCacheCreateInfo pcci;
	int i;

	if (mcc_vkr.ready)
		return 1;
	memset(dslb, 0, sizeof dslb);
	for (i = 0; i < 3; i++) {
		dslb[i].binding = (unsigned)i;
		dslb[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		dslb[i].descriptorCount = 1;
		dslb[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	memset(&dslci, 0, sizeof dslci);
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 3;
	dslci.pBindings = dslb;
	if (vkCreateDescriptorSetLayout(mcc_gpu.dev, &dslci, 0, &mcc_vkr.dsl) !=
			VK_SUCCESS)
		return 0;
	memset(&dps, 0, sizeof dps);
	dps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	dps.descriptorCount = 3;
	memset(&dpci, 0, sizeof dpci);
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = 1;
	dpci.poolSizeCount = 1;
	dpci.pPoolSizes = &dps;
	if (vkCreateDescriptorPool(mcc_gpu.dev, &dpci, 0, &mcc_vkr.dpool) !=
			VK_SUCCESS)
		return 0;
	memset(&dsai, 0, sizeof dsai);
	dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsai.descriptorPool = mcc_vkr.dpool;
	dsai.descriptorSetCount = 1;
	dsai.pSetLayouts = &mcc_vkr.dsl;
	if (vkAllocateDescriptorSets(mcc_gpu.dev, &dsai, &mcc_vkr.dset) != VK_SUCCESS)
		return 0;
	memset(&cpoolci, 0, sizeof cpoolci);
	cpoolci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpoolci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpoolci.queueFamilyIndex = mcc_gpu.qfam;
	if (vkCreateCommandPool(mcc_gpu.dev, &cpoolci, 0, &mcc_vkr.cpool) !=
			VK_SUCCESS)
		return 0;
	memset(&cbai, 0, sizeof cbai);
	cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbai.commandPool = mcc_vkr.cpool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = 1;
	if (vkAllocateCommandBuffers(mcc_gpu.dev, &cbai, &mcc_vkr.cb) != VK_SUCCESS)
		return 0;
	memset(&fci, 0, sizeof fci);
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if (vkCreateFence(mcc_gpu.dev, &fci, 0, &mcc_vkr.fence) != VK_SUCCESS)
		return 0;
	memset(&pcci, 0, sizeof pcci);
	pcci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	if (vkCreatePipelineCache(mcc_gpu.dev, &pcci, 0, &mcc_vkr.pcache) !=
			VK_SUCCESS)
		mcc_vkr.pcache = VK_NULL_HANDLE;
	mcc_vkr.ready = 1;
	return 1;
}

/* The shared address space: one host-mapped region every lane sees, holding the
 * globals image, the heap, and the printf ring. A pointer is a byte offset into
 * it, so offset 0 is reserved as NULL -- a bump allocator that handed out 0
 * would return a pointer equal to NULL, and malloc's result is null-checked at
 * every measured site.
 *
 * Host and device see the same bytes at command-buffer granularity, which is
 * all that is available: mid-kernel host->device writes are invisible by every
 * qualifier, so the host seeds this before the dispatch and drains it after,
 * never during. That is why the printf ring is device-writes-only. */
#define MCC_VK_MEM_DEFAULT (1u << 20)

static void mcc_vk_drop_mem(void) {
	if (!mcc_vkr.bmem)
		return;
	if (!mcc_vkr.memimported)
		vkUnmapMemory(mcc_gpu.dev, mcc_vkr.mmem);
	vkFreeMemory(mcc_gpu.dev, mcc_vkr.mmem, 0);
	vkDestroyBuffer(mcc_gpu.dev, mcc_vkr.bmem, 0);
	mcc_vkr.bmem = 0;
	mcc_vkr.mmem = 0;
	mcc_vkr.pmem = 0;
	mcc_vkr.bmemsz = 0;
	mcc_vkr.memimported = 0;
}

static int mcc_vk_bind_mem(VkDeviceSize want) {
	if (want <= mcc_vkr.bmemsz)
		return 1;
	if (mcc_vkr.memimported)
		return 0;
	mcc_vk_drop_mem();
	if (!mcc_gpu_buffer(want, &mcc_vkr.bmem, &mcc_vkr.mmem, &mcc_vkr.pmem))
		return 0;
	mcc_vkr.bmemsz = want;
	mcc_vkr.dsdirty = 1;
	memset(mcc_vkr.pmem, 0, (size_t)want);
	return 1;
}

static int mcc_vk_import_mem(void *p, VkDeviceSize size) {
	VkExternalMemoryBufferCreateInfo embci;
	VkBufferCreateInfo bci;
	VkMemoryRequirements mr;
	VkMemoryHostPointerPropertiesEXT hpp;
	VkImportMemoryHostPointerInfoEXT imhp;
	VkMemoryAllocateInfo mai;
	VkPhysicalDeviceMemoryProperties mp;
	VkBuffer buf = 0;
	VkDeviceMemory mem = 0;
	unsigned want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
									VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	unsigned mask, i;
	int idx = -1;

	if (!mcc_gpu.hostimp || !vkGetMemoryHostPointerPropertiesEXT)
		return 0;
	if (!p || !size)
		return 0;
	if ((uintptr_t)p & (mcc_gpu.hostimp_align - 1))
		return 0;
	if (size & (mcc_gpu.hostimp_align - 1))
		return 0;
	if (mcc_gpu.maxsbrange && size > mcc_gpu.maxsbrange)
		return 0;

	memset(&hpp, 0, sizeof hpp);
	hpp.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
	if (vkGetMemoryHostPointerPropertiesEXT(
					mcc_gpu.dev, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, p,
					&hpp) != VK_SUCCESS ||
			!hpp.memoryTypeBits)
		return 0;

	memset(&embci, 0, sizeof embci);
	embci.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
	embci.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
	memset(&bci, 0, sizeof bci);
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.pNext = &embci;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(mcc_gpu.dev, &bci, 0, &buf) != VK_SUCCESS)
		return 0;
	vkGetBufferMemoryRequirements(mcc_gpu.dev, buf, &mr);
	if (mr.size > size) {
		vkDestroyBuffer(mcc_gpu.dev, buf, 0);
		return 0;
	}
	mask = mr.memoryTypeBits & hpp.memoryTypeBits;
	vkGetPhysicalDeviceMemoryProperties(mcc_gpu.phys, &mp);
	for (i = 0; i < mp.memoryTypeCount; i++) {
		unsigned f = mp.memoryTypes[i].propertyFlags;
		if (!(mask & (1u << i)) || (f & want) != want)
			continue;
		if (idx < 0 || (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
			idx = (int)i;
		if (f & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
			break;
	}
	if (idx < 0) {
		vkDestroyBuffer(mcc_gpu.dev, buf, 0);
		return 0;
	}
	memset(&imhp, 0, sizeof imhp);
	imhp.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
	imhp.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
	imhp.pHostPointer = p;
	memset(&mai, 0, sizeof mai);
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.pNext = &imhp;
	mai.allocationSize = size;
	mai.memoryTypeIndex = (uint32_t)idx;
	if (vkAllocateMemory(mcc_gpu.dev, &mai, 0, &mem) != VK_SUCCESS) {
		vkDestroyBuffer(mcc_gpu.dev, buf, 0);
		return 0;
	}
	if (vkBindBufferMemory(mcc_gpu.dev, buf, mem, 0) != VK_SUCCESS) {
		vkFreeMemory(mcc_gpu.dev, mem, 0);
		vkDestroyBuffer(mcc_gpu.dev, buf, 0);
		return 0;
	}
	mcc_vk_drop_mem();
	mcc_vkr.bmem = buf;
	mcc_vkr.mmem = mem;
	mcc_vkr.pmem = p;
	mcc_vkr.bmemsz = size;
	mcc_vkr.memimported = 1;
	mcc_vkr.dsdirty = 1;
	if (mcc_vk_diag())
		fprintf(stderr, "[gpu-vk] imported host range %p+%lu as memory type %d\n", p,
						(unsigned long)size, idx);
	return 1;
}

static int mcc_vk_bind_buffers(VkDeviceSize insz, VkDeviceSize outsz) {
	VkDescriptorBufferInfo dbi[3];
	VkWriteDescriptorSet wds[3];
	int i, grew = 0;

	if (insz > mcc_vkr.binsz) {
		if (mcc_vkr.bin) {
			vkUnmapMemory(mcc_gpu.dev, mcc_vkr.min_);
			vkFreeMemory(mcc_gpu.dev, mcc_vkr.min_, 0);
			vkDestroyBuffer(mcc_gpu.dev, mcc_vkr.bin, 0);
			mcc_vkr.bin = 0;
		}
		if (!mcc_gpu_buffer(insz, &mcc_vkr.bin, &mcc_vkr.min_, &mcc_vkr.pin))
			return 0;
		mcc_vkr.binsz = insz;
		grew = 1;
	}
	if (outsz > mcc_vkr.boutsz) {
		if (mcc_vkr.bout) {
			vkUnmapMemory(mcc_gpu.dev, mcc_vkr.mout);
			vkFreeMemory(mcc_gpu.dev, mcc_vkr.mout, 0);
			vkDestroyBuffer(mcc_gpu.dev, mcc_vkr.bout, 0);
			mcc_vkr.bout = 0;
		}
		if (!mcc_gpu_buffer(outsz, &mcc_vkr.bout, &mcc_vkr.mout, &mcc_vkr.pout))
			return 0;
		mcc_vkr.boutsz = outsz;
		grew = 1;
	}
	if (!mcc_vkr.bmem) {
		if (!mcc_vk_bind_mem(MCC_VK_MEM_DEFAULT))
			return 0;
		grew = 1;
	}
	grew |= mcc_vkr.dsdirty;
	mcc_vkr.dsdirty = 0;
	if (!grew)
		return 1;
	memset(dbi, 0, sizeof dbi);
	dbi[0].buffer = mcc_vkr.bin;
	dbi[0].range = VK_WHOLE_SIZE;
	dbi[1].buffer = mcc_vkr.bout;
	dbi[1].range = VK_WHOLE_SIZE;
	dbi[2].buffer = mcc_vkr.bmem;
	dbi[2].range = VK_WHOLE_SIZE;
	memset(wds, 0, sizeof wds);
	for (i = 0; i < 3; i++) {
		wds[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds[i].dstSet = mcc_vkr.dset;
		wds[i].dstBinding = (unsigned)i;
		wds[i].descriptorCount = 1;
		wds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		wds[i].pBufferInfo = &dbi[i];
	}
	vkUpdateDescriptorSets(mcc_gpu.dev, 3, wds, 0, 0);
	return 1;
}

static MccVkPipe *mcc_vk_pipeline(const uint32_t *code, int nwords, int nlive) {
	VkShaderModuleCreateInfo smci;
	VkPipelineLayoutCreateInfo plci;
	VkComputePipelineCreateInfo cpci;
	uint64_t key = mcc_vk_key(code, nwords, nlive);
	MccVkPipe *e;
	int i;

	for (i = 0; i < mcc_vkr.ncache; i++)
		if (mcc_vkr.cache[i].key == key)
			return &mcc_vkr.cache[i];

	if (mcc_vkr.ncache < MCC_VK_CACHE_MAX) {
		e = &mcc_vkr.cache[mcc_vkr.ncache++];
	} else {
		e = &mcc_vkr.cache[mcc_vkr.next];
		mcc_vkr.next = (mcc_vkr.next + 1) % MCC_VK_CACHE_MAX;
		if (e->pipe)
			vkDestroyPipeline(mcc_gpu.dev, e->pipe, 0);
		if (e->play)
			vkDestroyPipelineLayout(mcc_gpu.dev, e->play, 0);
		if (e->sm)
			vkDestroyShaderModule(mcc_gpu.dev, e->sm, 0);
	}
	memset(e, 0, sizeof *e);

	memset(&smci, 0, sizeof smci);
	smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smci.codeSize = (size_t)nwords * 4;
	smci.pCode = code;
	if (vkCreateShaderModule(mcc_gpu.dev, &smci, 0, &e->sm) != VK_SUCCESS)
		return 0;
	memset(&plci, 0, sizeof plci);
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &mcc_vkr.dsl;
	if (vkCreatePipelineLayout(mcc_gpu.dev, &plci, 0, &e->play) != VK_SUCCESS)
		return 0;
	memset(&cpci, 0, sizeof cpci);
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cpci.stage.module = e->sm;
	cpci.stage.pName = "main";
	cpci.layout = e->play;
	if (vkCreateComputePipelines(mcc_gpu.dev, mcc_vkr.pcache, 1, &cpci, 0,
															 &e->pipe) != VK_SUCCESS)
		return 0;
	e->key = key;
	return e;
}

static void mcc_vk_release(void) {
	int i;
	if (!mcc_vkr.ready)
		return;
	for (i = 0; i < mcc_vkr.ncache; i++) {
		if (mcc_vkr.cache[i].pipe)
			vkDestroyPipeline(mcc_gpu.dev, mcc_vkr.cache[i].pipe, 0);
		if (mcc_vkr.cache[i].play)
			vkDestroyPipelineLayout(mcc_gpu.dev, mcc_vkr.cache[i].play, 0);
		if (mcc_vkr.cache[i].sm)
			vkDestroyShaderModule(mcc_gpu.dev, mcc_vkr.cache[i].sm, 0);
	}
	if (mcc_vkr.pcache)
		vkDestroyPipelineCache(mcc_gpu.dev, mcc_vkr.pcache, 0);
	if (mcc_vkr.fence)
		vkDestroyFence(mcc_gpu.dev, mcc_vkr.fence, 0);
	if (mcc_vkr.cpool)
		vkDestroyCommandPool(mcc_gpu.dev, mcc_vkr.cpool, 0);
	if (mcc_vkr.dpool)
		vkDestroyDescriptorPool(mcc_gpu.dev, mcc_vkr.dpool, 0);
	if (mcc_vkr.dsl)
		vkDestroyDescriptorSetLayout(mcc_gpu.dev, mcc_vkr.dsl, 0);
	if (mcc_vkr.bin) {
		vkUnmapMemory(mcc_gpu.dev, mcc_vkr.min_);
		vkFreeMemory(mcc_gpu.dev, mcc_vkr.min_, 0);
		vkDestroyBuffer(mcc_gpu.dev, mcc_vkr.bin, 0);
	}
	if (mcc_vkr.bout) {
		vkUnmapMemory(mcc_gpu.dev, mcc_vkr.mout);
		vkFreeMemory(mcc_gpu.dev, mcc_vkr.mout, 0);
		vkDestroyBuffer(mcc_gpu.dev, mcc_vkr.bout, 0);
	}
	mcc_vk_drop_mem();
	memset(&mcc_vkr, 0, sizeof mcc_vkr);
}

/* --- L2'(iii): the teardown --------------------------------------------- *
 * This used to be `closing = 1; if (ok && dev) vkDeviceWaitIdle(dev);` -- an
 * unbounded wait that destroyed nothing. Both halves were wrong in the same
 * direction: vkDeviceWaitIdle takes no timeout, so on a device that will never
 * go idle it does not return, and it is reached from an atexit handler
 * (ast_ladder_gpu_report), which is where cluster L wants to hang the JIT's
 * shutdown too.
 *
 * Bounding it does not need a timeout, it needs the wait to be unnecessary.
 * Every submission this module makes is on mcc_gpu.q, under mcc_gpu_lock, and
 * is followed before the lock is dropped by a fence wait with a finite,
 * caller-nameable timeout. So when quiesce takes the lock, the only submission
 * that can still be in flight is one whose fence wait did not succeed -- and
 * mcc_vkr.pending is raised exactly then. In every other case the fence has
 * already proved the queue idle and there is nothing left to wait for, which
 * is why vkDeviceWaitIdle is gone rather than merely guarded.
 *
 * What is destroyed is everything mcc_vk_resident and mcc_vk_bind_buffers
 * created, then the device and the instance. What is not destroyed is any of
 * that on a lost or stranded device, for the reason given at the fence-wait
 * failure in mcc_gpu_dispatch_locked2: a command buffer that will never
 * complete still references those objects, and handing them back to the driver
 * is worse than leaking them into process exit.
 *
 * mcc_gpu.ok is deliberately left alone. It is the answer to "did a device
 * come up", which is what mcc_gpu_stats reports -- and ast_ladder_gpu_report
 * calls this and then prints that. mcc_gpu_closing is what closes the door,
 * and it closes it for mcc_gpu_init too, so no path can reach a destroyed
 * handle. */
void mcc_gpu_quiesce(void) {
	MCC_GPU_LOCK();
	mcc_gpu_closing = 1;
	if (mcc_gpu.dev && mcc_gpu.ok && !mcc_gpu.lost && !mcc_gpu_ctr.stranded) {
		int idle = 1;
		if (mcc_vkr.pending) {
			idle = mcc_vk_chk(vkWaitForFences(mcc_gpu.dev, 1, &mcc_vkr.fence, VK_TRUE,
																				mcc_vk_fence_ns()),
												"vkWaitForFences") == VK_SUCCESS;
			if (idle)
				mcc_vkr.pending = 0;
			else
				mcc_gpu_ctr.stranded++;
		}
		if (idle) {
			mcc_vk_release();
			if (vkDestroyDevice)
				vkDestroyDevice(mcc_gpu.dev, 0);
			mcc_gpu.dev = 0;
			mcc_gpu.q = 0;
			if (mcc_gpu_inst && vkDestroyInstance)
				vkDestroyInstance(mcc_gpu_inst, 0);
			mcc_gpu_inst = 0;
			mcc_gpu.phys = 0;
			if (mcc_vk_diag())
				fprintf(stderr, "[gpu-vk] quiesce: released the resident objects and "
												"destroyed the device and the instance\n");
		} else if (mcc_vk_diag()) {
			fprintf(stderr, "[gpu-vk] quiesce: a submission is still pending after "
											"%llu ns; stranding the resident objects rather than "
											"waiting for a device that is not coming back\n",
							(unsigned long long)mcc_vk_fence_ns());
		}
	}
	MCC_GPU_UNLOCK();
}

int mcc_gpu_reopen(void) {
	MCC_GPU_LOCK();
	if (mcc_gpu.dev || mcc_gpu_inst || mcc_gpu.lost || mcc_gpu_ctr.stranded) {
		MCC_GPU_UNLOCK();
		return 0;
	}
	mcc_gpu_closing = 0;
	mcc_gpu.tried = 0;
	mcc_gpu.ok = 0;
	mcc_gpu.f64 = 0;
	mcc_gpu.hostimp = 0;
	mcc_gpu.hostimp_align = 0;
	mcc_gpu.hostimp_why[0] = 0;
	mcc_gpu.maxsbrange = 0;
	mcc_gpu.qfam = 0;
	mcc_gpu.name[0] = 0;
	MCC_GPU_UNLOCK();
	return mcc_gpu_init();
}

/* Set only for the duration of a frame dispatch, under the same lock that
 * serialises everything else here. */
static int32_t *mcc_gpu_rw_back;

static int mcc_gpu_dispatch_locked2(const uint32_t *code, int nwords,
																		const int32_t *in, int ntuple, int nlive,
																		int32_t *out, int reuse_in) {
	VkCommandBufferBeginInfo bi;
	VkSubmitInfo si;
	VkResult wr;
	MccVkPipe *pl;
	int cap = ((ntuple + MCC_GPU_LOCAL_SIZE - 1) / MCC_GPU_LOCAL_SIZE) *
						MCC_GPU_LOCAL_SIZE;

	if (!mcc_gpu_init())
		return 0;
	if (!mcc_vk_resident())
		return 0;
	if (!mcc_vk_bind_buffers((VkDeviceSize)cap * nlive * MCC_GPU_IN_SLOTS * 4,
													 (VkDeviceSize)cap * MCC_GPU_OUT_SLOTS * 4))
		return 0;
	pl = mcc_vk_pipeline(code, nwords, nlive);
	if (!pl)
		return 0;

	/* Only the [ntuple, cap) padding tail needs clearing, and only on the input
	 * side: every lane below cap writes all three out slots unconditionally, and
	 * out is read only for t < ntuple, so zeroing pout was 100% dead. Zeroing the
	 * whole of pin was 32 B/lane of duplicated work, since the memcpy immediately
	 * overwrites the [0, ntuple) prefix. The mapping is write-combined, so these
	 * stores are not free. */
	if (!reuse_in) {
		memcpy(mcc_vkr.pin, in, (size_t)ntuple * nlive * MCC_GPU_IN_SLOTS * 4);
		if (cap > ntuple)
			memset((char *)mcc_vkr.pin +
								 (size_t)ntuple * nlive * MCC_GPU_IN_SLOTS * 4,
						 0, (size_t)(cap - ntuple) * nlive * MCC_GPU_IN_SLOTS * 4);
	}

	if (vkResetFences(mcc_gpu.dev, 1, &mcc_vkr.fence) != VK_SUCCESS)
		return 0;
	if (vkResetCommandBuffer(mcc_vkr.cb, 0) != VK_SUCCESS)
		return 0;
	memset(&bi, 0, sizeof bi);
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(mcc_vkr.cb, &bi) != VK_SUCCESS)
		return 0;
	vkCmdBindPipeline(mcc_vkr.cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl->pipe);
	vkCmdBindDescriptorSets(mcc_vkr.cb, VK_PIPELINE_BIND_POINT_COMPUTE, pl->play,
													0, 1, &mcc_vkr.dset, 0, 0);
	vkCmdDispatch(mcc_vkr.cb, (unsigned)(cap / MCC_GPU_LOCAL_SIZE), 1, 1);
	if (vkEndCommandBuffer(mcc_vkr.cb) != VK_SUCCESS)
		return 0;
	memset(&si, 0, sizeof si);
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &mcc_vkr.cb;
	if (mcc_vk_chk(vkQueueSubmit(mcc_gpu.q, 1, &si, mcc_vkr.fence),
								 "vkQueueSubmit") != VK_SUCCESS)
		return 0;
	mcc_vkr.pending = 1;
	wr = mcc_vk_chk(vkWaitForFences(mcc_gpu.dev, 1, &mcc_vkr.fence, VK_TRUE,
																	mcc_vk_fence_ns()),
									"vkWaitForFences");
	if (wr != VK_SUCCESS) {
		/* The command buffer is still pending and every resident object is
		 * referenced by it. Touching any of them hands the driver memory a zombie
		 * kernel is still writing to, which is how a timeout in dispatch N used to
		 * silently corrupt dispatch N+1. Strand instead: the device is marked dead
		 * here, so the leak is bounded at one process's worth of objects and no
		 * further dispatch can occur. mcc_vkr.pending stays raised, which is what
		 * stops mcc_gpu_quiesce from destroying them or waiting on them at exit. */
		if (mcc_vk_diag())
			fprintf(stderr,
							"[gpu-vk] fence wait failed rc=%d after %llu ns; stranding the "
							"resident objects and disabling the device\n",
							(int)wr, (unsigned long long)mcc_vk_fence_ns());
		mcc_gpu.ok = 0;
		mcc_gpu_ctr.stranded++;
		return 0;
	}
	mcc_vkr.pending = 0;
	if (out)
		memcpy(out, mcc_vkr.pout, (size_t)ntuple * MCC_GPU_OUT_SLOTS * 4);
	if (mcc_gpu_rw_back)
		memcpy(mcc_gpu_rw_back, mcc_vkr.pin,
					 (size_t)ntuple * nlive * MCC_GPU_IN_SLOTS * 4);
	mcc_gpu_ctr.dispatches++;
	mcc_gpu_ctr.lanes += ntuple;
	return 1;
}


static int mcc_gpu_backend_load(void) { return mcc_vk_load(); }

#define MCC_GPU_CODE_PTR(p) ((const uint32_t *)(p))
#define MCC_GPU_IN_IS_RESIDENT 1

static int mcc_gpu_rw_supported(void) { return 1; }

static void mcc_gpu_rw_arm(int32_t *p) { mcc_gpu_rw_back = p; }

static int mcc_gpu_mem_backend(void **base, unsigned long *size) {
	if (!mcc_gpu_init() || !mcc_vk_resident())
		return 0;
	if (!mcc_vkr.memimported && !mcc_vk_bind_mem(MCC_VK_MEM_DEFAULT))
		return 0;
	if (base)
		*base = mcc_vkr.pmem;
	if (size)
		*size = (unsigned long)mcc_vkr.bmemsz;
	return 1;
}

static unsigned long mcc_gpu_host_import_align_backend(const char **why) {
	if (!mcc_gpu_init()) {
		if (why)
			*why = "no usable Vulkan device";
		return 0;
	}
	if (!mcc_gpu.hostimp) {
		if (why)
			*why = mcc_gpu.hostimp_why;
		return 0;
	}
	if (why)
		*why = "";
	return mcc_gpu.hostimp_align;
}

static int mcc_gpu_mem_import_backend(void *base, unsigned long size) {
	if (!mcc_gpu_init() || !mcc_vk_resident())
		return 0;
	if (size > 0x7ffffffcul)
		return 0;
	if (!mcc_vk_import_mem(base, (VkDeviceSize)size))
		return 0;
	return mcc_vk_bind_buffers(mcc_vkr.binsz ? mcc_vkr.binsz : 4,
														 mcc_vkr.boutsz ? mcc_vkr.boutsz : 4);
}

#endif /* MCC_GPU_LANG_MSL */

int mcc_gpu_dispatch_rw2(const void *code, int n, int32_t *inout, int ntuple,
												 int nslot, int32_t *out) {
	int rc;
	MCC_GPU_LOCK();
	if (!mcc_gpu_backend_load()) {
		MCC_GPU_UNLOCK();
		return 0;
	}
	MCC_GPU_UNLOCK();
	MCC_GPU_LOCK();
	if (mcc_gpu_closing || !mcc_gpu_rw_supported()) {
		MCC_GPU_UNLOCK();
		return 0;
	}
	mcc_gpu_rw_arm(inout);
	rc = mcc_gpu_dispatch_locked2(MCC_GPU_CODE_PTR(code), n, inout, ntuple, nslot,
																out, 0);
	mcc_gpu_rw_arm(NULL);
	MCC_GPU_UNLOCK();
	return rc;
}

int mcc_gpu_dispatch_rw(const void *code, int n, int32_t *inout, int ntuple,
												int nslot) {
	return mcc_gpu_dispatch_rw2(code, n, inout, ntuple, nslot, NULL);
}

/* The host's view of the shared address space. Valid between dispatches only:
 * the device sees these bytes at command-buffer granularity, so seeding must
 * happen before submit and draining after completion, never during. */
int mcc_gpu_mem(void **base, unsigned long *size) {
	int rc = 0;
	MCC_GPU_LOCK();
	if (!mcc_gpu_backend_load()) {
		MCC_GPU_UNLOCK();
		return 0;
	}
	rc = mcc_gpu_mem_backend(base, size);
	MCC_GPU_UNLOCK();
	return rc;
}

unsigned long mcc_gpu_host_import_align(const char **why) {
	unsigned long rc;
	if (why)
		*why = "the GPU backend did not load";
	MCC_GPU_LOCK();
	if (!mcc_gpu_backend_load()) {
		MCC_GPU_UNLOCK();
		return 0;
	}
	rc = mcc_gpu_host_import_align_backend(why);
	MCC_GPU_UNLOCK();
	return rc;
}

int mcc_gpu_mem_import(void *base, unsigned long size) {
	int rc;
	MCC_GPU_LOCK();
	if (!mcc_gpu_backend_load()) {
		MCC_GPU_UNLOCK();
		return 0;
	}
	rc = mcc_gpu_mem_import_backend(base, size);
	MCC_GPU_UNLOCK();
	return rc;
}

int mcc_gpu_alive(void) { return mcc_gpu.ok; }

int mcc_gpu_f64(void) { return mcc_gpu.ok && mcc_gpu.f64; }

long mcc_gpu_stranded(void) { return mcc_gpu_ctr.stranded; }

void mcc_gpu_stats(MccGpuStats *out) {
	out->tried = mcc_gpu.tried;
	out->ok = mcc_gpu.ok;
	out->name = mcc_gpu.ok ? mcc_gpu.name : "(none)";
	out->dispatches = mcc_gpu_ctr.dispatches;
	out->lanes = mcc_gpu_ctr.lanes;
}

int mcc_gpu_dispatch(const void *code, int n, const int32_t *in, int ntuple,
										 int nlive, int32_t *out) {
	int rc, ready;
	fenv_t mcc_gpu_fe;
	int mcc_gpu_fe_ok;
	MCC_GPU_LOCK();
	ready = mcc_gpu_backend_load();
	MCC_GPU_UNLOCK();
	if (!ready)
		return 0;
	mcc_gpu_fe_ok = (mcc_fe_get(&mcc_gpu_fe) == 0);
	MCC_GPU_LOCK();
	rc = mcc_gpu_closing ? 0
											 : mcc_gpu_dispatch_locked2(MCC_GPU_CODE_PTR(code), n, in,
																									ntuple, nlive, out, 0);
	MCC_GPU_UNLOCK();
	if (mcc_gpu_fe_ok)
		mcc_fe_set(&mcc_gpu_fe);
	return rc;
}

int mcc_gpu_dispatch2_ro_in(const void *ca, int na, const void *cb, int nb,
														const int32_t *in, int ntuple, int nlive,
														int32_t *oa, int32_t *ob) {
	int rc, ready;
	fenv_t mcc_gpu_fe;
	int mcc_gpu_fe_ok;
	MCC_GPU_LOCK();
	ready = mcc_gpu_backend_load();
	MCC_GPU_UNLOCK();
	if (!ready)
		return 0;
	mcc_gpu_fe_ok = (mcc_fe_get(&mcc_gpu_fe) == 0);
	MCC_GPU_LOCK();
	rc = mcc_gpu_closing
					 ? 0
					 : mcc_gpu_dispatch_locked2(MCC_GPU_CODE_PTR(ca), na, in, ntuple,
																			nlive, oa, 0);
	if (rc)
		rc = mcc_gpu_dispatch_locked2(MCC_GPU_CODE_PTR(cb), nb, in, ntuple, nlive,
																	ob, MCC_GPU_IN_IS_RESIDENT);
	MCC_GPU_UNLOCK();
	if (mcc_gpu_fe_ok)
		mcc_fe_set(&mcc_gpu_fe);
	return rc;
}
