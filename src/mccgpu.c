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

#if MCC_GPU_LANG_MSL

#include <objc/message.h>
#include <objc/runtime.h>

#define MCC_MTL_UTF8 4
#define MCC_MTL_CACHE_MAX 64

typedef id (*MccMtlCreateDeviceFn)(void);

static MccMtlCreateDeviceFn mcc_mtl_create_device;
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

void mcc_gpu_quiesce(void) {
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
	int cap = ((ntuple + MCC_GPU_LOCAL_SIZE - 1) / MCC_GPU_LOCAL_SIZE) * MCC_GPU_LOCAL_SIZE;
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

static int mcc_gpu_backend_load(void) { return mcc_mtl_load(); }

#define MCC_GPU_CODE_PTR(p) ((const char *)(p))

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
typedef struct VkPhysicalDeviceFeatures VkPhysicalDeviceFeatures;
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
	VK_STRUCTURE_TYPE_MAX_ENUM = 0x7FFFFFFF
} VkStructureType;

typedef enum VkPhysicalDeviceType {
	VK_PHYSICAL_DEVICE_TYPE_OTHER = 0,
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
#define VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 0x00000002
#define VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 0x00000004
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

typedef VkResult(VKAPI_PTR *PFN_vkCreateInstance)(
		const VkInstanceCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkInstance *pInstance);
typedef VkResult(VKAPI_PTR *PFN_vkEnumeratePhysicalDevices)(
		VkInstance instance, uint32_t *pPhysicalDeviceCount,
		VkPhysicalDevice *pPhysicalDevices);
typedef void(VKAPI_PTR *PFN_vkGetPhysicalDeviceProperties)(
		VkPhysicalDevice physicalDevice, VkPhysicalDeviceProperties *pProperties);
typedef void(VKAPI_PTR *PFN_vkGetPhysicalDeviceQueueFamilyProperties)(
		VkPhysicalDevice physicalDevice, uint32_t *pQueueFamilyPropertyCount,
		VkQueueFamilyProperties *pQueueFamilyProperties);
typedef void(VKAPI_PTR *PFN_vkGetPhysicalDeviceMemoryProperties)(
		VkPhysicalDevice physicalDevice,
		VkPhysicalDeviceMemoryProperties *pMemoryProperties);
typedef VkResult(VKAPI_PTR *PFN_vkCreateDevice)(
		VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
		const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);
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
typedef VkResult(VKAPI_PTR *PFN_vkWaitForFences)(VkDevice device,
																								 uint32_t fenceCount,
																								 const VkFence *pFences,
																								 VkBool32 waitAll,
																								 uint64_t timeout);

#define MCC_VK_FNS(X)                                                          \
	X(vkCreateInstance)                                                          \
	X(vkEnumeratePhysicalDevices)                                                \
	X(vkGetPhysicalDeviceProperties)                                             \
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
	X(vkWaitForFences)

#define MCC_VK_DECL(n) static PFN_##n n;
MCC_VK_FNS(MCC_VK_DECL)
#undef MCC_VK_DECL

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

static const char *const mcc_vk_sonames[] = {
#if MCC_HOST_WIN32
		"vulkan-1.dll",
#else
		"libvulkan.so.1", "libvulkan.so", "libvulkan.dylib",
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
	if (!mcc_fe_bind())
		return 0;
	mcc_vk_ok = 1;
	return 1;
}

typedef struct MccGpu {
	int tried;
	int ok;
	VkInstance inst;
	VkPhysicalDevice phys;
	VkDevice dev;
	VkQueue q;
	unsigned qfam;
	char name[256];
	long dispatches;
	long lanes;
} MccGpu;

static MccGpu mcc_gpu;

static int mcc_gpu_init(void) {
	VkApplicationInfo ai;
	VkInstanceCreateInfo ici;
	VkPhysicalDevice devs[8];
	VkQueueFamilyProperties qf[32];
	VkPhysicalDeviceProperties props;
	VkDeviceQueueCreateInfo qci;
	VkDeviceCreateInfo dci;
	float prio = 1.0f;
	unsigned ndev = 8, nq = 32, i;

	if (mcc_gpu_closing)
		return 0;
	if (mcc_gpu.tried)
		return mcc_gpu.ok;
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
		VkResult _r = vkCreateInstance(&ici, 0, &mcc_gpu.inst);
		if (_r != VK_SUCCESS) {
			if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
				fprintf(stderr, "[ladder-gpu] vkCreateInstance rc=%d\n", (int)_r);
			return 0;
		}
	}
	{
		VkResult _r = vkEnumeratePhysicalDevices(mcc_gpu.inst, &ndev, devs);
		if (_r != VK_SUCCESS || !ndev) {
			if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
				fprintf(stderr, "[ladder-gpu] vkEnumeratePhysicalDevices rc=%d ndev=%u\n",
								(int)_r, ndev);
			return 0;
		}
	}
	mcc_gpu.phys = devs[0];
	vkGetPhysicalDeviceProperties(mcc_gpu.phys, &props);
	snprintf(mcc_gpu.name, sizeof mcc_gpu.name, "%s", props.deviceName);
	mcc_gpu.qfam = 0xFFFFFFFFu;
	vkGetPhysicalDeviceQueueFamilyProperties(mcc_gpu.phys, &nq, qf);
	for (i = 0; i < nq; i++)
		if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			mcc_gpu.qfam = i;
			break;
		}
	if (mcc_gpu.qfam == 0xFFFFFFFFu) {
		if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
			fprintf(stderr, "[ladder-gpu] no compute queue (nq=%u)\n", nq);
		return 0;
	}
	if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
		fprintf(stderr, "[ladder-gpu] init ok dev=%s qfam=%u\n", mcc_gpu.name,
						mcc_gpu.qfam);
	memset(&qci, 0, sizeof qci);
	qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qci.queueFamilyIndex = mcc_gpu.qfam;
	qci.queueCount = 1;
	qci.pQueuePriorities = &prio;
	memset(&dci, 0, sizeof dci);
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	{
		VkResult _r = vkCreateDevice(mcc_gpu.phys, &dci, 0, &mcc_gpu.dev);
		if (_r != VK_SUCCESS) {
			if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG"))
				fprintf(stderr, "[ladder-gpu] vkCreateDevice rc=%d\n", (int)_r);
			return 0;
		}
	}
	vkGetDeviceQueue(mcc_gpu.dev, mcc_gpu.qfam, 0, &mcc_gpu.q);
	mcc_gpu.ok = 1;
	return 1;
}

void mcc_gpu_quiesce(void) {
	MCC_GPU_LOCK();
	mcc_gpu_closing = 1;
	if (mcc_gpu.ok && mcc_gpu.dev)
		vkDeviceWaitIdle(mcc_gpu.dev);
	MCC_GPU_UNLOCK();
}

static int mcc_gpu_mem_index(VkMemoryRequirements mr, uint32_t *out) {
	VkPhysicalDeviceMemoryProperties mp;
	unsigned i;
	unsigned want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
									VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	vkGetPhysicalDeviceMemoryProperties(mcc_gpu.phys, &mp);
	for (i = 0; i < mp.memoryTypeCount; i++)
		if ((mr.memoryTypeBits & (1u << i)) &&
				(mp.memoryTypes[i].propertyFlags & want) == want) {
			*out = i;
			return 1;
		}
	return 0;
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
			vkMapMemory(mcc_gpu.dev, *mem, 0, size, 0, map) != VK_SUCCESS) {
		vkFreeMemory(mcc_gpu.dev, *mem, 0);
		vkDestroyBuffer(mcc_gpu.dev, *buf, 0);
		return 0;
	}
	return 1;
}

static int mcc_gpu_dispatch_locked(const uint32_t *code, int nwords,
																	 const int32_t *in, int ntuple, int nlive,
																	 int32_t *out) {
	VkBuffer bin, bout;
	VkDeviceMemory min_, mout;
	void *pin, *pout;
	VkDescriptorSetLayoutBinding dslb[2];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkDescriptorSetLayout dsl = 0;
	VkDescriptorPoolSize dps;
	VkDescriptorPoolCreateInfo dpci;
	VkDescriptorPool dpool = 0;
	VkDescriptorSetAllocateInfo dsai;
	VkDescriptorSet dset;
	VkDescriptorBufferInfo dbi[2];
	VkWriteDescriptorSet wds[2];
	VkShaderModuleCreateInfo smci;
	VkShaderModule sm = 0;
	VkPipelineLayoutCreateInfo plci;
	VkPipelineLayout play = 0;
	VkComputePipelineCreateInfo cpci;
	VkPipeline pipe = 0;
	VkCommandPoolCreateInfo cpoolci;
	VkCommandPool cpool = 0;
	VkCommandBufferAllocateInfo cbai;
	VkCommandBuffer cb;
	VkCommandBufferBeginInfo bi;
	VkSubmitInfo si;
	VkFenceCreateInfo fci;
	VkFence fence = 0;
	int i, rc = 0;
	int cap = ((ntuple + MCC_GPU_LOCAL_SIZE - 1) / MCC_GPU_LOCAL_SIZE) * MCC_GPU_LOCAL_SIZE;

	if (!mcc_gpu_init())
		return 0;
	if (!mcc_gpu_buffer((VkDeviceSize)cap * nlive * 4, &bin, &min_, &pin))
		return 0;
	if (!mcc_gpu_buffer((VkDeviceSize)cap * 2 * 4, &bout, &mout, &pout)) {
		vkUnmapMemory(mcc_gpu.dev, min_);
		vkFreeMemory(mcc_gpu.dev, min_, 0);
		vkDestroyBuffer(mcc_gpu.dev, bin, 0);
		return 0;
	}
	memset(pin, 0, (size_t)cap * nlive * 4);
	memcpy(pin, in, (size_t)ntuple * nlive * 4);
	memset(pout, 0, (size_t)cap * 2 * 4);

	memset(dslb, 0, sizeof dslb);
	for (i = 0; i < 2; i++) {
		dslb[i].binding = (unsigned)i;
		dslb[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		dslb[i].descriptorCount = 1;
		dslb[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	}
	memset(&dslci, 0, sizeof dslci);
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 2;
	dslci.pBindings = dslb;
	if (vkCreateDescriptorSetLayout(mcc_gpu.dev, &dslci, 0, &dsl) != VK_SUCCESS)
		goto done;
	memset(&dps, 0, sizeof dps);
	dps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	dps.descriptorCount = 2;
	memset(&dpci, 0, sizeof dpci);
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = 1;
	dpci.poolSizeCount = 1;
	dpci.pPoolSizes = &dps;
	if (vkCreateDescriptorPool(mcc_gpu.dev, &dpci, 0, &dpool) != VK_SUCCESS)
		goto done;
	memset(&dsai, 0, sizeof dsai);
	dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsai.descriptorPool = dpool;
	dsai.descriptorSetCount = 1;
	dsai.pSetLayouts = &dsl;
	if (vkAllocateDescriptorSets(mcc_gpu.dev, &dsai, &dset) != VK_SUCCESS)
		goto done;
	memset(dbi, 0, sizeof dbi);
	dbi[0].buffer = bin;
	dbi[0].range = VK_WHOLE_SIZE;
	dbi[1].buffer = bout;
	dbi[1].range = VK_WHOLE_SIZE;
	memset(wds, 0, sizeof wds);
	for (i = 0; i < 2; i++) {
		wds[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds[i].dstSet = dset;
		wds[i].dstBinding = (unsigned)i;
		wds[i].descriptorCount = 1;
		wds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		wds[i].pBufferInfo = &dbi[i];
	}
	vkUpdateDescriptorSets(mcc_gpu.dev, 2, wds, 0, 0);

	memset(&smci, 0, sizeof smci);
	smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smci.codeSize = (size_t)nwords * 4;
	smci.pCode = code;
	if (vkCreateShaderModule(mcc_gpu.dev, &smci, 0, &sm) != VK_SUCCESS)
		goto done;
	memset(&plci, 0, sizeof plci);
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &dsl;
	if (vkCreatePipelineLayout(mcc_gpu.dev, &plci, 0, &play) != VK_SUCCESS)
		goto done;
	memset(&cpci, 0, sizeof cpci);
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cpci.stage.module = sm;
	cpci.stage.pName = "main";
	cpci.layout = play;
	if (vkCreateComputePipelines(mcc_gpu.dev, VK_NULL_HANDLE, 1, &cpci, 0,
															 &pipe) != VK_SUCCESS)
		goto done;
	memset(&cpoolci, 0, sizeof cpoolci);
	cpoolci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpoolci.queueFamilyIndex = mcc_gpu.qfam;
	if (vkCreateCommandPool(mcc_gpu.dev, &cpoolci, 0, &cpool) != VK_SUCCESS)
		goto done;
	memset(&cbai, 0, sizeof cbai);
	cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbai.commandPool = cpool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = 1;
	if (vkAllocateCommandBuffers(mcc_gpu.dev, &cbai, &cb) != VK_SUCCESS)
		goto done;
	memset(&fci, 0, sizeof fci);
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	if (vkCreateFence(mcc_gpu.dev, &fci, 0, &fence) != VK_SUCCESS)
		goto done;
	memset(&bi, 0, sizeof bi);
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(cb, &bi) != VK_SUCCESS)
		goto done;
	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, play, 0, 1, &dset,
													0, 0);
	vkCmdDispatch(cb, (unsigned)(cap / MCC_GPU_LOCAL_SIZE), 1, 1);
	if (vkEndCommandBuffer(cb) != VK_SUCCESS)
		goto done;
	memset(&si, 0, sizeof si);
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cb;
	if (vkQueueSubmit(mcc_gpu.q, 1, &si, fence) != VK_SUCCESS)
		goto done;
	if (vkWaitForFences(mcc_gpu.dev, 1, &fence, VK_TRUE,
											30ULL * 1000000000ULL) != VK_SUCCESS)
		goto done;
	memcpy(out, pout, (size_t)ntuple * 2 * 4);
	mcc_gpu.dispatches++;
	mcc_gpu.lanes += ntuple;
	rc = 1;

done:
	if (fence)
		vkDestroyFence(mcc_gpu.dev, fence, 0);
	if (cpool)
		vkDestroyCommandPool(mcc_gpu.dev, cpool, 0);
	if (pipe)
		vkDestroyPipeline(mcc_gpu.dev, pipe, 0);
	if (play)
		vkDestroyPipelineLayout(mcc_gpu.dev, play, 0);
	if (sm)
		vkDestroyShaderModule(mcc_gpu.dev, sm, 0);
	if (dpool)
		vkDestroyDescriptorPool(mcc_gpu.dev, dpool, 0);
	if (dsl)
		vkDestroyDescriptorSetLayout(mcc_gpu.dev, dsl, 0);
	vkUnmapMemory(mcc_gpu.dev, min_);
	vkUnmapMemory(mcc_gpu.dev, mout);
	vkFreeMemory(mcc_gpu.dev, min_, 0);
	vkFreeMemory(mcc_gpu.dev, mout, 0);
	vkDestroyBuffer(mcc_gpu.dev, bin, 0);
	vkDestroyBuffer(mcc_gpu.dev, bout, 0);
	return rc;
}

static int mcc_gpu_backend_load(void) { return mcc_vk_load(); }

#define MCC_GPU_CODE_PTR(p) ((const uint32_t *)(p))

#endif /* MCC_GPU_LANG_MSL */

void mcc_gpu_stats(MccGpuStats *out) {
	out->tried = mcc_gpu.tried;
	out->ok = mcc_gpu.ok;
	out->name = mcc_gpu.ok ? mcc_gpu.name : "(none)";
	out->dispatches = mcc_gpu.dispatches;
	out->lanes = mcc_gpu.lanes;
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
											 : mcc_gpu_dispatch_locked(MCC_GPU_CODE_PTR(code), n, in,
																								 ntuple, nlive, out);
	MCC_GPU_UNLOCK();
	if (mcc_gpu_fe_ok)
		mcc_fe_set(&mcc_gpu_fe);
	return rc;
}
