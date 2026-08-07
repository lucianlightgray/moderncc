#ifndef MCC_COMPUTE_BACKEND_PROVIDED
#define MCC_COMPUTE_BACKEND_PROVIDED 1

#include <fenv.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>

#include "mcchost.h"

#if MCC_HOST_POSIX
#include <pthread.h>
static pthread_mutex_t mcc_gpu_lock = PTHREAD_MUTEX_INITIALIZER;
#define MCC_GPU_LOCK() pthread_mutex_lock(&mcc_gpu_lock)
#define MCC_GPU_UNLOCK() pthread_mutex_unlock(&mcc_gpu_lock)
#else
#define MCC_GPU_LOCK() ((void)0)
#define MCC_GPU_UNLOCK() ((void)0)
#endif

#define MCC_GPU_CODE_MAX 65536
#define MCC_GPU_CODE_SUFFIX "metal"
#define MCC_MTL_UTF8 4
#define MCC_MTL_CACHE_MAX 64

typedef id (*MccMtlCreateDeviceFn)(void);

static MccMtlCreateDeviceFn mcc_mtl_create_device;
static int mcc_mtl_tried;
static int mcc_mtl_ok;

typedef int (*MccFeGetFn)(fenv_t *);
typedef int (*MccFeSetFn)(const fenv_t *);

static MccFeGetFn mcc_fe_get;
static MccFeSetFn mcc_fe_set;

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
	char name[256];
	long dispatches;
	long lanes;
} MccGpu;

typedef struct MtlPipe {
	uint64_t key;
	int len;
	id pso;
} MtlPipe;

static MccGpu mcc_gpu;
static int mcc_gpu_closing;
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

static int mcc_gpu_init(void) {
	id pool, nm;

	if (mcc_gpu_closing)
		return 0;
	if (mcc_gpu.tried)
		return mcc_gpu.ok;
	mcc_gpu.tried = 1;
	if (!mcc_mtl_load())
		return 0;
	pool = mtl_send(mtl_send((id)objc_getClass("NSAutoreleasePool"), "alloc"),
									"init");
	mcc_gpu.dev = mcc_mtl_create_device();
	if (!mcc_gpu.dev) {
		if (mtl_diag())
			fprintf(stderr, "[ladder-gpu] MTLCreateSystemDefaultDevice returned nil\n");
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
	nm = mtl_send(mcc_gpu.dev, "name");
	snprintf(mcc_gpu.name, sizeof mcc_gpu.name, "%s", mtl_utf8(nm));
	if (mtl_diag())
		fprintf(stderr, "[ladder-gpu] init ok dev=%s\n", mcc_gpu.name);
	mtl_send_v(pool, "drain");
	mcc_gpu.ok = 1;
	return 1;
}

static void mcc_gpu_quiesce(void) {
	int i;
	MCC_GPU_LOCK();
	mcc_gpu_closing = 1;
	for (i = 0; i < mcc_mtl_cache_n; i++)
		mtl_release(mcc_mtl_cache[i].pso);
	mcc_mtl_cache_n = 0;
	MCC_GPU_UNLOCK();
}

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
			(id)0, &err);
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

static int mcc_gpu_dispatch_locked(const char *src, int len, const int32_t *in,
																	 int ntuple, int nlive, int32_t *out) {
	id pool, pso, bin, bout, cb, enc;
	void *pin, *pout;
	MtlSize grid, tg;
	int cap = ((ntuple + MSL_LOCAL_SIZE - 1) / MSL_LOCAL_SIZE) * MSL_LOCAL_SIZE;
	int rc = 0;

	if (!mcc_gpu_init())
		return 0;
	pool = mtl_send(mtl_send((id)objc_getClass("NSAutoreleasePool"), "alloc"),
									"init");
	pso = mtl_pipeline(src, len);
	if (!pso)
		goto done;
	bin = mtl_buffer((unsigned long)cap * nlive * 4, &pin);
	if (!bin)
		goto done;
	bout = mtl_buffer((unsigned long)cap * 2 * 4, &pout);
	if (!bout) {
		mtl_release(bin);
		goto done;
	}
	memset(pin, 0, (size_t)cap * nlive * 4);
	memcpy(pin, in, (size_t)ntuple * nlive * 4);
	memset(pout, 0, (size_t)cap * 2 * 4);

	cb = mtl_send(mcc_gpu.queue, "commandBuffer");
	enc = cb ? mtl_send(cb, "computeCommandEncoder") : 0;
	if (!enc) {
		mtl_report_err("computeCommandEncoder", 0);
		mtl_release(bin);
		mtl_release(bout);
		goto done;
	}
	((void (*)(id, SEL, id))objc_msgSend)(
			enc, sel_registerName("setComputePipelineState:"), pso);
	((void (*)(id, SEL, id, unsigned long, unsigned long))objc_msgSend)(
			enc, sel_registerName("setBuffer:offset:atIndex:"), bin, 0, 0);
	((void (*)(id, SEL, id, unsigned long, unsigned long))objc_msgSend)(
			enc, sel_registerName("setBuffer:offset:atIndex:"), bout, 0, 1);
	grid.w = (unsigned long)(cap / MSL_LOCAL_SIZE);
	grid.h = 1;
	grid.d = 1;
	tg.w = MSL_LOCAL_SIZE;
	tg.h = 1;
	tg.d = 1;
	((void (*)(id, SEL, MtlSize, MtlSize))objc_msgSend)(
			enc, sel_registerName("dispatchThreadgroups:threadsPerThreadgroup:"), grid,
			tg);
	mtl_send_v(enc, "endEncoding");
	mtl_send_v(cb, "commit");
	mtl_send_v(cb, "waitUntilCompleted");
	memcpy(out, pout, (size_t)ntuple * 2 * 4);
	mcc_gpu.dispatches++;
	mcc_gpu.lanes += ntuple;
	rc = 1;
	mtl_release(bin);
	mtl_release(bout);

done:
	mtl_send_v(pool, "drain");
	return rc;
}

static int mcc_gpu_dispatch(const char *src, int len, const int32_t *in,
														int ntuple, int nlive, int32_t *out) {
	int rc, ready;
	fenv_t mcc_gpu_fe;
	int mcc_gpu_fe_ok;
	MCC_GPU_LOCK();
	ready = mcc_mtl_load();
	MCC_GPU_UNLOCK();
	if (!ready)
		return 0;
	mcc_gpu_fe_ok = (mcc_fe_get(&mcc_gpu_fe) == 0);
	MCC_GPU_LOCK();
	rc = mcc_gpu_closing
					 ? 0
					 : mcc_gpu_dispatch_locked(src, len, in, ntuple, nlive, out);
	MCC_GPU_UNLOCK();
	if (mcc_gpu_fe_ok)
		mcc_fe_set(&mcc_gpu_fe);
	return rc;
}

typedef struct MccGpuCode {
	void *p;
	int n;
} MccGpuCode;

static int mcc_gpu_emit(AstArena *a, AstLocal root, const int32_t *off, int n,
												MccGpuCode *c) {
	MslMod m;
	uint32_t base, val, lane;
	char *src;
	int nb = 0;
	msl_module_begin(&m, n);
	base = msl_main_begin(&m, n);
	if (!msl_expr(&m, a, root, off, n, base, &val) || m.failed) {
		msl_module_free(&m);
		return 0;
	}
	lane = msl_lane(&m, base, n);
	msl_main_end(&m, lane, val);
	src = msl_module_finish(&m, &nb);
	msl_module_free(&m);
	if (nb > MCC_GPU_CODE_MAX) {
		MSL_FREE(src);
		return 0;
	}
	c->p = src;
	c->n = nb;
	return 1;
}

static void mcc_gpu_code_free(MccGpuCode *c) {
	MSL_FREE(c->p);
	c->p = NULL;
	c->n = 0;
}

static void mcc_gpu_code_dump(const MccGpuCode *c, const char *path) {
	FILE *fp = fopen(path, "wb");
	if (fp) {
		fwrite(c->p, 1, (size_t)c->n, fp);
		fclose(fp);
	}
}

static int mcc_gpu_run(const MccGpuCode *c, const int32_t *in, int ntuple,
											 int nlive, int32_t *out) {
	return mcc_gpu_dispatch((const char *)c->p, c->n, in, ntuple, nlive, out);
}

#endif
