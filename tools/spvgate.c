#include "mcc.h"
#include "mccast.h"

ST_INLN int is_float(int t) {
	int bt = t & VT_BTYPE;
	return bt == VT_LDOUBLE || bt == VT_DOUBLE || bt == VT_FLOAT ||
				 bt == VT_QFLOAT || bt == VT_FLOAT16;
}

static int ast_bad_type(int tt) {
	int bt = tt & VT_BTYPE;
	return bt == VT_STRUCT || (tt & VT_BITFIELD) || bt == VT_LDOUBLE ||
				 bt == VT_QFLOAT || bt == VT_INT128;
}

#undef malloc
#undef realloc
#undef free
#undef strdup

#include "ast_eval_slice.h"

/* One source, two gates, chosen by SPVGATE_MSL on the compile line; it drives
 * the same MCC_GPU_LANG_MSL switch mccgpu.h uses to pick an emitter, and must
 * override whatever the directory-scope define says, because the two arms are
 * built side by side out of one tree.  The SPIR-V arm is the default on every
 * host including Darwin: it brings its own vulkan/vulkan.h and its own device
 * plumbing, so it must not pull in mccgpu.c's vendored Vulkan ABI.  The Metal
 * arm has no such conflict and dispatches through mccgpu.c itself. */
#ifndef SPVGATE_MSL
#define SPVGATE_MSL 0
#endif
#undef MCC_GPU_LANG_MSL
#define MCC_GPU_LANG_MSL SPVGATE_MSL
#define MCC_GPU_EMITTER 1
#include "mccgpu.h"

#ifndef AST_EVAL_SLICE_PROVIDED
#error "spvgate needs the real ast_eval_slice; the mccast.c fallback stub returns 1 without writing *out, so every comparison would read uninitialised memory and pass."
#endif

#include <stdio.h>
#include <string.h>

#if MCC_GPU_LANG_MSL
#define GATE_NAME "mslgate"
typedef MslMod GateMod;
typedef MslV GateV;
typedef char GateUnit;
#define gate_module_begin msl_module_begin
#define gate_main_begin msl_main_begin
#define gate_expr msl_expr
#define gate_main_end msl_main_end
#define gate_module_finish msl_module_finish
#define gate_module_free msl_module_free
#define gate_mutate msl_mutate
#else
#define GATE_NAME "spvgate"
typedef SpvMod GateMod;
typedef SpvV GateV;
typedef uint32_t GateUnit;
#define gate_module_begin spv_module_begin
#define gate_main_begin spv_main_begin
#define gate_expr spv_expr
#define gate_main_end spv_main_end
#define gate_module_finish spv_module_finish
#define gate_module_free spv_module_free
#define gate_mutate spv_mutate
#endif

#if !MCC_GPU_LANG_MSL
#include <vulkan/vulkan.h>

#define VK(x)                                                              \
	do {                                                                     \
		VkResult _r = (x);                                                     \
		if (_r != VK_SUCCESS) {                                                \
			printf("spvgate: vulkan call failed line %d rc=%d\n", __LINE__,       \
						 (int)_r);                                                     \
			exit(1);                                                             \
		}                                                                      \
	} while (0)

#define VK_HOST(x)                                                         \
	do {                                                                     \
		VkResult _r = (x);                                                     \
		if (_r != VK_SUCCESS) {                                                \
			printf("spvgate: no usable vulkan host, line %d rc=%d\n", __LINE__,  \
						 (int)_r);                                                     \
			exit(77);                                                            \
		}                                                                      \
	} while (0)

/* Same, but VK_INCOMPLETE is a success. It is only ever used on the *fill* half
 * of a count-then-fill pair, where it means the array was too small to list
 * every device -- ndev is still the number written and this gate uses devs[0].
 * The count query keeps plain VK_HOST: a loader that answers VK_INCOMPLETE to a
 * NULL-array query is out of spec and the handles it goes on to write are not
 * valid (AMD's VK_LAYER_AMD_switchable_graphics does this, and using devs[0]
 * from it faults in vkGetPhysicalDeviceProperties). Skipping is right there. */
#define VK_HOST_PARTIAL(x)                                                 \
	do {                                                                     \
		VkResult _r = (x);                                                     \
		if (_r != VK_SUCCESS && _r != VK_INCOMPLETE) {                         \
			printf("spvgate: no usable vulkan host, line %d rc=%d\n", __LINE__,  \
						 (int)_r);                                                     \
			exit(77);                                                            \
		}                                                                      \
	} while (0)
#endif

#define MAX_LIVE 8
#define MAX_TUPLES (1 << 20)

static void put_in(int32_t *p, long i, int64_t v) {
	p[i * MCC_GPU_IN_SLOTS] = (int32_t)(uint32_t)(uint64_t)v;
	p[i * MCC_GPU_IN_SLOTS + 1] = (int32_t)(uint32_t)((uint64_t)v >> 32);
}

static int64_t get_in(const int32_t *p, long i) {
	uint64_t lo = (uint32_t)p[i * MCC_GPU_IN_SLOTS];
	uint64_t hi = (uint32_t)p[i * MCC_GPU_IN_SLOTS + 1];
	return (int64_t)(lo | (hi << 32));
}

static int64_t get_out(const int32_t *p, long t) {
	uint64_t lo = (uint32_t)p[t * MCC_GPU_OUT_SLOTS];
	uint64_t hi = (uint32_t)p[t * MCC_GPU_OUT_SLOTS + 1];
	return (int64_t)(lo | (hi << 32));
}

static int get_def(const int32_t *p, long t) {
	return p[t * MCC_GPU_OUT_SLOTS + 2] != 0;
}

static char g_devname[256];
static long g_dispatches;
static long g_lanes;
static int g_f64;
static long g_f64_skipped;

#if MCC_GPU_LANG_MSL

ST_FUNC void *host_dlopen(const char *name) {
	return dlopen(name, RTLD_GLOBAL | RTLD_LAZY);
}

ST_FUNC const char *host_dlerror(void) { return dlerror(); }

ST_FUNC void *host_dlsym(void *h, const char *symbol) {
	return dlsym(h, symbol);
}

ST_FUNC void *host_dlsym_process(const char *symbol) {
	return dlsym(RTLD_DEFAULT, symbol);
}

static MslV msl_mutate(MslMod *m, MslV v) {
	uint32_t p = msl_pair(m, v);
	return msl_mk(msl_pv(m, "int2(p%u.x ^ 1, p%u.y)", p, p), 1, 0);
}

static void gpu_init(void) {
	MslMod m;
	MccGpuStats gs;
	int32_t in[MCC_GPU_IN_SLOTS], out[MCC_GPU_OUT_SLOTS];
	uint32_t base;
	MslV v;
	char *src;
	int nb = 0;

	msl_module_begin(&m, 1);
	base = msl_main_begin(&m, 1);
	v = msl_load_live_v(&m, base, 0, 1, 0);
	msl_main_end(&m, m.lane, v);
	src = msl_module_finish(&m, &nb);
	msl_module_free(&m);
	memset(in, 0, sizeof in);
	if (!mcc_gpu_dispatch(src, nb, in, 1, 1, out)) {
		free(src);
		printf(GATE_NAME ": no usable metal host\n");
		exit(77);
	}
	free(src);
	mcc_gpu_stats(&gs);
	snprintf(g_devname, sizeof g_devname, "%s", gs.name);
}

static int gpu_run(const char *code, int nbytes, const int32_t *in, int ntuple,
									 int nlive, int32_t *out) {
	if (!mcc_gpu_dispatch(code, nbytes, in, ntuple, nlive, out))
		return -1;
	g_dispatches++;
	g_lanes += ntuple;
	return 0;
}

#else

static VkInstance g_inst;
static VkPhysicalDevice g_phys;
static VkDevice g_dev;
static VkQueue g_q;
static unsigned g_qfam;

static void gpu_init(void) {
	VkApplicationInfo ai;
	VkInstanceCreateInfo ici;
	VkPhysicalDevice devs[8];
	VkQueueFamilyProperties qf[32];
	VkPhysicalDeviceProperties props;
	VkDeviceQueueCreateInfo qci;
	VkDeviceCreateInfo dci;
	VkPhysicalDeviceFeatures feat;
	float prio = 1.0f;
	unsigned ndev = 0, nq = 32, i;

	memset(&ai, 0, sizeof ai);
	ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	ai.pApplicationName = "spvgate";
	ai.apiVersion = VK_API_VERSION_1_1;
	memset(&ici, 0, sizeof ici);
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.pApplicationInfo = &ai;
	VK_HOST(vkCreateInstance(&ici, 0, &g_inst));
	/* Count, then fill. The count query has to succeed outright; VK_INCOMPLETE
	 * from the fill just means there are more devices than devs[] holds. */
	VK_HOST(vkEnumeratePhysicalDevices(g_inst, &ndev, NULL));
	if (!ndev) {
		printf("spvgate: no vulkan device\n");
		exit(77);
	}
	if (ndev > (unsigned)(sizeof devs / sizeof devs[0]))
		ndev = (unsigned)(sizeof devs / sizeof devs[0]);
	VK_HOST_PARTIAL(vkEnumeratePhysicalDevices(g_inst, &ndev, devs));
	if (!ndev) {
		printf("spvgate: no vulkan device\n");
		exit(77);
	}
	g_phys = devs[0];
	vkGetPhysicalDeviceProperties(g_phys, &props);
	snprintf(g_devname, sizeof g_devname, "%s", props.deviceName);
	g_qfam = 0xFFFFFFFFu;
	vkGetPhysicalDeviceQueueFamilyProperties(g_phys, &nq, qf);
	for (i = 0; i < nq; i++)
		if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			g_qfam = i;
			break;
		}
	if (g_qfam == 0xFFFFFFFFu) {
		printf("spvgate: no compute queue\n");
		exit(77);
	}
	memset(&qci, 0, sizeof qci);
	qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qci.queueFamilyIndex = g_qfam;
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
		vkGetPhysicalDeviceFeatures(g_phys, &have);
		g_f64 = have.shaderFloat64 ? 1 : 0;
		feat.shaderFloat64 = have.shaderFloat64;
	}
	dci.pEnabledFeatures = &feat;
	VK_HOST(vkCreateDevice(g_phys, &dci, 0, &g_dev));
	vkGetDeviceQueue(g_dev, g_qfam, 0, &g_q);
}

static uint32_t mem_index(VkMemoryRequirements mr) {
	VkPhysicalDeviceMemoryProperties mp;
	unsigned i;
	unsigned want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
									VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	vkGetPhysicalDeviceMemoryProperties(g_phys, &mp);
	for (i = 0; i < mp.memoryTypeCount; i++)
		if ((mr.memoryTypeBits & (1u << i)) &&
				(mp.memoryTypes[i].propertyFlags & want) == want)
			return i;
	printf("spvgate: no host-visible memory\n");
	exit(1);
}

static void make_buffer(VkDeviceSize size, VkBuffer *buf, VkDeviceMemory *mem,
												void **map) {
	VkBufferCreateInfo bci;
	VkMemoryRequirements mr;
	VkMemoryAllocateInfo mai;
	memset(&bci, 0, sizeof bci);
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK(vkCreateBuffer(g_dev, &bci, 0, buf));
	vkGetBufferMemoryRequirements(g_dev, *buf, &mr);
	memset(&mai, 0, sizeof mai);
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mem_index(mr);
	VK(vkAllocateMemory(g_dev, &mai, 0, mem));
	VK(vkBindBufferMemory(g_dev, *buf, *mem, 0));
	VK(vkMapMemory(g_dev, *mem, 0, size, 0, map));
}

#define SPVGATE_MEM_BYTES (1u << 20)

static VkBuffer g_membuf;
static VkDeviceMemory g_memmem;
static void *g_memmap;

static void *mem_window(unsigned long *size) {
	if (!g_memmap) {
		make_buffer((VkDeviceSize)SPVGATE_MEM_BYTES, &g_membuf, &g_memmem,
								&g_memmap);
		memset(g_memmap, 0, SPVGATE_MEM_BYTES);
	}
	if (size)
		*size = SPVGATE_MEM_BYTES;
	return g_memmap;
}

static int gpu_run(const uint32_t *code, int nwords, const int32_t *in,
									 int ntuple, int nlive, int32_t *out) {
	VkBuffer bin, bout;
	VkDeviceMemory min_, mout;
	void *pin, *pout;
	VkDescriptorSetLayoutBinding dslb[3];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkDescriptorSetLayout dsl;
	VkDescriptorPoolSize dps;
	VkDescriptorPoolCreateInfo dpci;
	VkDescriptorPool dpool;
	VkDescriptorSetAllocateInfo dsai;
	VkDescriptorSet dset;
	VkDescriptorBufferInfo dbi[3];
	VkWriteDescriptorSet wds[3];
	VkShaderModuleCreateInfo smci;
	VkShaderModule sm;
	VkPipelineLayoutCreateInfo plci;
	VkPipelineLayout play;
	VkComputePipelineCreateInfo cpci;
	VkPipeline pipe;
	VkCommandPoolCreateInfo cpoolci;
	VkCommandPool cpool;
	VkCommandBufferAllocateInfo cbai;
	VkCommandBuffer cb;
	VkCommandBufferBeginInfo bi;
	VkSubmitInfo si;
	VkFenceCreateInfo fci;
	VkFence fence;
	VkResult rc;
	int i;

	int cap = ((ntuple + SPV_LOCAL_SIZE - 1) / SPV_LOCAL_SIZE) * SPV_LOCAL_SIZE;
	make_buffer((VkDeviceSize)cap * nlive * MCC_GPU_IN_SLOTS * 4, &bin, &min_,
							&pin);
	make_buffer((VkDeviceSize)cap * MCC_GPU_OUT_SLOTS * 4, &bout, &mout, &pout);
	memset(pin, 0, (size_t)cap * nlive * MCC_GPU_IN_SLOTS * 4);
	memcpy(pin, in, (size_t)ntuple * nlive * MCC_GPU_IN_SLOTS * 4);
	memset(pout, 0, (size_t)cap * MCC_GPU_OUT_SLOTS * 4);

	mem_window(NULL);

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
	VK(vkCreateDescriptorSetLayout(g_dev, &dslci, 0, &dsl));

	memset(&dps, 0, sizeof dps);
	dps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	dps.descriptorCount = 3;
	memset(&dpci, 0, sizeof dpci);
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.maxSets = 1;
	dpci.poolSizeCount = 1;
	dpci.pPoolSizes = &dps;
	VK(vkCreateDescriptorPool(g_dev, &dpci, 0, &dpool));
	memset(&dsai, 0, sizeof dsai);
	dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsai.descriptorPool = dpool;
	dsai.descriptorSetCount = 1;
	dsai.pSetLayouts = &dsl;
	VK(vkAllocateDescriptorSets(g_dev, &dsai, &dset));

	memset(dbi, 0, sizeof dbi);
	dbi[0].buffer = bin;
	dbi[0].range = VK_WHOLE_SIZE;
	dbi[1].buffer = bout;
	dbi[1].range = VK_WHOLE_SIZE;
	dbi[2].buffer = g_membuf;
	dbi[2].range = VK_WHOLE_SIZE;
	memset(wds, 0, sizeof wds);
	for (i = 0; i < 3; i++) {
		wds[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds[i].dstSet = dset;
		wds[i].dstBinding = (unsigned)i;
		wds[i].descriptorCount = 1;
		wds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		wds[i].pBufferInfo = &dbi[i];
	}
	vkUpdateDescriptorSets(g_dev, 3, wds, 0, 0);

	memset(&smci, 0, sizeof smci);
	smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smci.codeSize = (size_t)nwords * 4;
	smci.pCode = code;
	rc = vkCreateShaderModule(g_dev, &smci, 0, &sm);
	if (rc != VK_SUCCESS) {
		vkDestroyDescriptorPool(g_dev, dpool, 0);
		vkDestroyDescriptorSetLayout(g_dev, dsl, 0);
		vkUnmapMemory(g_dev, min_);
		vkUnmapMemory(g_dev, mout);
		vkFreeMemory(g_dev, min_, 0);
		vkFreeMemory(g_dev, mout, 0);
		vkDestroyBuffer(g_dev, bin, 0);
		vkDestroyBuffer(g_dev, bout, 0);
		return -1;
	}

	memset(&plci, 0, sizeof plci);
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &dsl;
	VK(vkCreatePipelineLayout(g_dev, &plci, 0, &play));
	memset(&cpci, 0, sizeof cpci);
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	cpci.stage.module = sm;
	cpci.stage.pName = "main";
	cpci.layout = play;
	rc = vkCreateComputePipelines(g_dev, VK_NULL_HANDLE, 1, &cpci, 0, &pipe);
	if (rc != VK_SUCCESS) {
		vkDestroyPipelineLayout(g_dev, play, 0);
		vkDestroyShaderModule(g_dev, sm, 0);
		vkDestroyDescriptorPool(g_dev, dpool, 0);
		vkDestroyDescriptorSetLayout(g_dev, dsl, 0);
		vkUnmapMemory(g_dev, min_);
		vkUnmapMemory(g_dev, mout);
		vkFreeMemory(g_dev, min_, 0);
		vkFreeMemory(g_dev, mout, 0);
		vkDestroyBuffer(g_dev, bin, 0);
		vkDestroyBuffer(g_dev, bout, 0);
		return -1;
	}

	memset(&cpoolci, 0, sizeof cpoolci);
	cpoolci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpoolci.queueFamilyIndex = g_qfam;
	VK(vkCreateCommandPool(g_dev, &cpoolci, 0, &cpool));
	memset(&cbai, 0, sizeof cbai);
	cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbai.commandPool = cpool;
	cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cbai.commandBufferCount = 1;
	VK(vkAllocateCommandBuffers(g_dev, &cbai, &cb));
	memset(&fci, 0, sizeof fci);
	fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VK(vkCreateFence(g_dev, &fci, 0, &fence));

	memset(&bi, 0, sizeof bi);
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK(vkBeginCommandBuffer(cb, &bi));
	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, play, 0, 1, &dset,
													0, 0);
	vkCmdDispatch(cb, (unsigned)((ntuple + SPV_LOCAL_SIZE - 1) / SPV_LOCAL_SIZE), 1,
								1);
	VK(vkEndCommandBuffer(cb));
	memset(&si, 0, sizeof si);
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &cb;
	VK(vkQueueSubmit(g_q, 1, &si, fence));
	VK(vkWaitForFences(g_dev, 1, &fence, VK_TRUE, 10ULL * 1000000000ULL));
	memcpy(out, pout, (size_t)ntuple * MCC_GPU_OUT_SLOTS * 4);
	g_dispatches++;
	g_lanes += ntuple;

	vkDestroyFence(g_dev, fence, 0);
	vkDestroyCommandPool(g_dev, cpool, 0);
	vkDestroyPipeline(g_dev, pipe, 0);
	vkDestroyPipelineLayout(g_dev, play, 0);
	vkDestroyShaderModule(g_dev, sm, 0);
	vkDestroyDescriptorPool(g_dev, dpool, 0);
	vkDestroyDescriptorSetLayout(g_dev, dsl, 0);
	vkUnmapMemory(g_dev, min_);
	vkUnmapMemory(g_dev, mout);
	vkFreeMemory(g_dev, min_, 0);
	vkFreeMemory(g_dev, mout, 0);
	vkDestroyBuffer(g_dev, bin, 0);
	vkDestroyBuffer(g_dev, bout, 0);
	return 0;
}

#endif

static AstLocal mk(AstArena *a, uint16_t kind, int t) {
	AstLocal n = ast_node(a, kind);
	ast_set_type(a, n, t, 0);
	return n;
}

static AstLocal mk_lit(AstArena *a, int64_t v, int t) {
	AstLocal n = mk(a, AST_Literal, t);
	ast_set_op(a, n, VT_CONST);
	ast_set_ival(a, n, (uint64_t)v);
	return n;
}

static AstLocal mk_ref(AstArena *a, int off, int t) {
	AstLocal n = mk(a, AST_Ref, t);
	ast_set_op(a, n, VT_LOCAL);
	ast_set_ival(a, n, (uint64_t)(int64_t)off);
	return n;
}

static AstLocal mk_bin(AstArena *a, int op, AstLocal x, AstLocal y, int t) {
	AstLocal n = mk(a, AST_Binary, t);
	ast_set_op(a, n, op);
	ast_add_child(a, n, x);
	ast_add_child(a, n, y);
	return n;
}

static AstLocal mk_un(AstArena *a, int op, AstLocal x, int t) {
	AstLocal n = mk(a, AST_Unary, t);
	ast_set_op(a, n, op);
	ast_add_child(a, n, x);
	return n;
}

typedef struct Case {
	const char *name;
	int nlive;
	AstLocal (*build)(AstArena *a, const int *o);
} Case;

static AstLocal b_addmul(AstArena *a, const int *o) {
	return mk_bin(a, '+', mk_bin(a, '*', mk_ref(a, o[0], VT_INT),
															 mk_lit(a, 3, VT_INT), VT_INT),
								mk_ref(a, o[1], VT_INT), VT_INT);
}

static AstLocal b_shifts(AstArena *a, const int *o) {
	AstLocal x = mk_ref(a, o[0], VT_INT);
	AstLocal s = mk_bin(a, '&', mk_ref(a, o[1], VT_INT), mk_lit(a, 31, VT_INT),
											VT_INT);
	return mk_bin(a, '^', mk_bin(a, TOK_SHL, x, s, VT_INT),
								mk_bin(a, TOK_SAR, x, mk_lit(a, 3, VT_INT), VT_INT), VT_INT);
}

static AstLocal mk_odd(AstArena *a, int off, int t) {
	return mk_bin(a, '|', mk_ref(a, off, t), mk_lit(a, 1, t), t);
}

static AstLocal b_divmod(AstArena *a, const int *o) {
	return mk_bin(a, '+',
								mk_bin(a, '/', mk_ref(a, o[0], VT_INT),
											 mk_odd(a, o[1], VT_INT), VT_INT),
								mk_bin(a, '%', mk_ref(a, o[0], VT_INT),
											 mk_odd(a, o[1], VT_INT), VT_INT),
								VT_INT);
}

static AstLocal b_udivmod(AstArena *a, const int *o) {
	int u = VT_INT | VT_UNSIGNED;
	return mk_bin(a, '+',
								mk_bin(a, '/', mk_ref(a, o[0], u), mk_odd(a, o[1], u), u),
								mk_bin(a, '%', mk_ref(a, o[0], u), mk_odd(a, o[1], u), u), u);
}

static AstLocal b_cmp(AstArena *a, const int *o) {
	return mk_bin(a, '+',
								mk_bin(a, TOK_LT, mk_ref(a, o[0], VT_INT),
											 mk_ref(a, o[1], VT_INT), VT_INT),
								mk_bin(a, TOK_EQ, mk_ref(a, o[0], VT_INT),
											 mk_ref(a, o[1], VT_INT), VT_INT),
								VT_INT);
}

static AstLocal b_cmpu(AstArena *a, const int *o) {
	int u = VT_INT | VT_UNSIGNED;
	return mk_bin(a, '+',
								mk_bin(a, TOK_LT, mk_ref(a, o[0], u), mk_ref(a, o[1], u), u),
								mk_bin(a, TOK_GT, mk_ref(a, o[0], u), mk_ref(a, o[1], u), u),
								u);
}

static AstLocal b_cmpx(AstArena *a, const int *o) {
	return mk_bin(a, '+',
								mk_bin(a, TOK_ULT, mk_ref(a, o[0], VT_INT),
											 mk_ref(a, o[1], VT_INT), VT_INT),
								mk_bin(a, TOK_GE, mk_ref(a, o[0], VT_INT),
											 mk_ref(a, o[1], VT_INT), VT_INT),
								VT_INT);
}

static AstLocal b_narrow(AstArena *a, const int *o) {
	AstLocal c = mk(a, AST_Convert, VT_BYTE);
	ast_add_child(a, c, mk_ref(a, o[0], VT_INT));
	AstLocal c2 = mk(a, AST_Convert, VT_SHORT | VT_UNSIGNED);
	ast_add_child(a, c2, mk_ref(a, o[1], VT_INT));
	return mk_bin(a, '*', c, c2, VT_INT);
}

static AstLocal b_ternary(AstArena *a, const int *o) {
	AstLocal n = mk(a, AST_If, VT_INT);
	AstLocal d = mk_ref(a, o[1], VT_INT);
	ast_add_child(a, n, d);
	ast_add_child(a, n, mk_bin(a, '/', mk_ref(a, o[0], VT_INT), d, VT_INT));
	ast_add_child(a, n, mk_lit(a, -1, VT_INT));
	return n;
}

static AstLocal b_land(AstArena *a, const int *o) {
	AstLocal n = mk(a, AST_Binary, VT_INT);
	ast_set_op(a, n, TOK_LAND);
	ast_add_child(a, n, mk_ref(a, o[1], VT_INT));
	ast_add_child(a, n,
								mk_bin(a, '>', mk_bin(a, '/', mk_ref(a, o[0], VT_INT),
																			mk_ref(a, o[1], VT_INT), VT_INT),
											 mk_lit(a, 2, VT_INT), VT_INT));
	return n;
}

static AstLocal b_notneg(AstArena *a, const int *o) {
	return mk_bin(a, '^', mk_un(a, '~', mk_ref(a, o[0], VT_INT), VT_INT),
								mk_un(a, '-', mk_ref(a, o[1], VT_INT), VT_INT), VT_INT);
}

static AstLocal b_sdiv(AstArena *a, const int *o) {
	return mk_bin(a, '/', mk_ref(a, o[0], VT_INT), mk_odd(a, o[1], VT_INT),
								VT_INT);
}

static AstLocal b_srem(AstArena *a, const int *o) {
	return mk_bin(a, '%', mk_ref(a, o[0], VT_INT), mk_odd(a, o[1], VT_INT),
								VT_INT);
}

static AstLocal b_divraw(AstArena *a, const int *o) {
	return mk_bin(a, '/', mk_ref(a, o[0], VT_INT), mk_ref(a, o[1], VT_INT),
								VT_INT);
}

static AstLocal b_remraw(AstArena *a, const int *o) {
	return mk_bin(a, '%', mk_ref(a, o[0], VT_INT), mk_ref(a, o[1], VT_INT),
								VT_INT);
}

static AstLocal b_shiftraw(AstArena *a, const int *o) {
	return mk_bin(a, TOK_SHL, mk_ref(a, o[0], VT_INT), mk_ref(a, o[1], VT_INT),
								VT_INT);
}

static AstLocal b_ovf(AstArena *a, const int *o) {
	return mk_bin(a, '*', mk_ref(a, o[0], VT_INT), mk_ref(a, o[1], VT_INT),
								VT_INT);
}

static AstLocal b_ovfadd(AstArena *a, const int *o) {
	return mk_bin(a, '+',
								mk_bin(a, '*', mk_ref(a, o[0], VT_INT),
											 mk_lit(a, 65536, VT_INT), VT_INT),
								mk_bin(a, '*', mk_ref(a, o[1], VT_INT),
											 mk_lit(a, 65536, VT_INT), VT_INT),
								VT_INT);
}

#if !MCC_GPU_LANG_MSL
static SpvV spv_mutate(SpvMod *m, SpvV v) {
	uint32_t p = spv_pair(m, v);
	uint32_t lo = spv_uop(m, SpvOpBitwiseXor, spv_lo(m, p), spv_uintc(m, 1));
	return spv_mk(spv_u2(m, lo, spv_hi(m, p)), 1, 0);
}
#endif

#define VT_LL VT_LLONG
#define VT_ULL (VT_LLONG | VT_UNSIGNED)

static AstLocal mk_cvt(AstArena *a, AstLocal x, int t) {
	AstLocal n = mk(a, AST_Convert, t);
	ast_add_child(a, n, x);
	return n;
}

static AstLocal mk_up(AstArena *a, int off, int t, int sh) {
	return mk_bin(a, TOK_SHL, mk_ref(a, off, t), mk_lit(a, sh, VT_INT), t);
}

static AstLocal b_ll_addsub(AstArena *a, const int *o) {
	return mk_bin(a, '-', mk_up(a, o[0], VT_LL, 40), mk_up(a, o[1], VT_LL, 33),
								VT_LL);
}

static AstLocal b_ll_add(AstArena *a, const int *o) {
	return mk_bin(a, '+', mk_up(a, o[0], VT_LL, 47), mk_up(a, o[1], VT_LL, 47),
								VT_LL);
}

static AstLocal b_ll_mul(AstArena *a, const int *o) {
	return mk_bin(a, '*', mk_up(a, o[0], VT_LL, 31), mk_up(a, o[1], VT_LL, 17),
								VT_LL);
}

static AstLocal b_ll_umul(AstArena *a, const int *o) {
	return mk_bin(a, '*', mk_up(a, o[0], VT_ULL, 31), mk_up(a, o[1], VT_ULL, 17),
								VT_ULL);
}

static AstLocal b_ll_divraw(AstArena *a, const int *o) {
	return mk_bin(a, '/', mk_up(a, o[0], VT_LL, 63), mk_ref(a, o[1], VT_LL),
								VT_LL);
}

static AstLocal b_ll_remraw(AstArena *a, const int *o) {
	return mk_bin(a, '%', mk_up(a, o[0], VT_LL, 63), mk_ref(a, o[1], VT_LL),
								VT_LL);
}

static AstLocal b_ll_divmod(AstArena *a, const int *o) {
	AstLocal d = mk_bin(a, '|', mk_ref(a, o[1], VT_LL), mk_lit(a, 1, VT_LL),
											VT_LL);
	return mk_bin(a, '+',
								mk_bin(a, '/', mk_up(a, o[0], VT_LL, 40), d, VT_LL),
								mk_bin(a, '%', mk_up(a, o[0], VT_LL, 37), d, VT_LL), VT_LL);
}

static AstLocal b_ll_udivmod(AstArena *a, const int *o) {
	AstLocal d = mk_bin(a, '|', mk_ref(a, o[1], VT_ULL), mk_lit(a, 1, VT_ULL),
											VT_ULL);
	return mk_bin(a, '+',
								mk_bin(a, '/', mk_up(a, o[0], VT_ULL, 40), d, VT_ULL),
								mk_bin(a, '%', mk_up(a, o[0], VT_ULL, 37), d, VT_ULL), VT_ULL);
}

static AstLocal b_ll_shiftraw(AstArena *a, const int *o) {
	return mk_bin(a, TOK_SHL, mk_up(a, o[0], VT_LL, 40), mk_ref(a, o[1], VT_LL),
								VT_LL);
}

static AstLocal b_ll_shifts(AstArena *a, const int *o) {
	AstLocal sh = mk_bin(a, '&', mk_ref(a, o[1], VT_LL), mk_lit(a, 63, VT_LL),
											 VT_LL);
	AstLocal x = mk_up(a, o[0], VT_LL, 40);
	return mk_bin(a, '^', mk_bin(a, TOK_SHL, x, sh, VT_LL),
								mk_bin(a, TOK_SAR, x, sh, VT_LL), VT_LL);
}

static AstLocal b_ll_ushifts(AstArena *a, const int *o) {
	AstLocal sh = mk_bin(a, '&', mk_ref(a, o[1], VT_ULL), mk_lit(a, 63, VT_ULL),
											 VT_ULL);
	AstLocal x = mk_up(a, o[0], VT_ULL, 40);
	return mk_bin(a, '^', mk_bin(a, TOK_SHL, x, sh, VT_ULL),
								mk_bin(a, TOK_SHR, x, sh, VT_ULL), VT_ULL);
}

static AstLocal b_ll_cmp(AstArena *a, const int *o) {
	AstLocal x = mk_up(a, o[0], VT_LL, 40), y = mk_up(a, o[1], VT_LL, 40);
	return mk_bin(a, '+', mk_bin(a, TOK_LT, x, y, VT_LL),
								mk_bin(a, TOK_EQ, x, y, VT_LL), VT_LL);
}

static AstLocal b_ll_cmpu(AstArena *a, const int *o) {
	AstLocal x = mk_up(a, o[0], VT_ULL, 40), y = mk_up(a, o[1], VT_ULL, 40);
	return mk_bin(a, '+', mk_bin(a, TOK_LT, x, y, VT_ULL),
								mk_bin(a, TOK_GE, x, y, VT_ULL), VT_ULL);
}

static AstLocal b_ll_bits(AstArena *a, const int *o) {
	AstLocal x = mk_up(a, o[0], VT_LL, 40), y = mk_up(a, o[1], VT_LL, 21);
	return mk_bin(a, '^', mk_bin(a, '&', x, y, VT_LL),
								mk_bin(a, '|', x, y, VT_LL), VT_LL);
}

static AstLocal b_ll_notneg(AstArena *a, const int *o) {
	return mk_bin(a, '^', mk_un(a, '~', mk_up(a, o[0], VT_LL, 40), VT_LL),
								mk_un(a, '-', mk_up(a, o[1], VT_LL, 63), VT_LL), VT_LL);
}

static AstLocal b_ll_narrow(AstArena *a, const int *o) {
	AstLocal x = mk_cvt(a, mk_up(a, o[0], VT_LL, 33), VT_INT);
	AstLocal y = mk_cvt(a, mk_ref(a, o[1], VT_INT), VT_LL);
	return mk_bin(a, '*', mk_cvt(a, x, VT_LL), y, VT_LL);
}

static AstLocal b_ll_mix(AstArena *a, const int *o) {
	AstLocal x = mk_cvt(a, mk_ref(a, o[0], VT_INT), VT_LL);
	return mk_bin(a, '+', mk_bin(a, TOK_SHL, x, mk_lit(a, 40, VT_INT), VT_LL),
								mk_ref(a, o[1], VT_LL), VT_LL);
}

static AstLocal b_ll_ternary(AstArena *a, const int *o) {
	AstLocal n = mk(a, AST_If, VT_LL);
	AstLocal d = mk_ref(a, o[1], VT_LL);
	ast_add_child(a, n, d);
	ast_add_child(a, n, mk_bin(a, '/', mk_up(a, o[0], VT_LL, 40), d, VT_LL));
	ast_add_child(a, n, mk_lit(a, -1, VT_LL));
	return n;
}

static AstLocal b_ll_land(AstArena *a, const int *o) {
	AstLocal n = mk(a, AST_Binary, VT_INT);
	ast_set_op(a, n, TOK_LAND);
	ast_add_child(a, n, mk_ref(a, o[1], VT_LL));
	ast_add_child(a, n,
								mk_bin(a, '>',
											 mk_bin(a, '/', mk_up(a, o[0], VT_LL, 40),
															mk_ref(a, o[1], VT_LL), VT_LL),
											 mk_lit(a, 2, VT_LL), VT_LL));
	return n;
}

static AstLocal b_ll_bool(AstArena *a, const int *o) {
	AstLocal x = mk_cvt(a, mk_up(a, o[0], VT_LL, 40), VT_BOOL);
	AstLocal y = mk_un(a, '!', mk_up(a, o[1], VT_LL, 40), VT_INT);
	return mk_bin(a, '+', x, y, VT_INT);
}

static const Case CASES[] = {
		{"divraw", 2, b_divraw},   {"remraw", 2, b_remraw},
		{"shiftraw", 2, b_shiftraw}, {"ovf", 2, b_ovf},
		{"ovfadd", 2, b_ovfadd},
		{"sdiv", 2, b_sdiv},       {"srem", 2, b_srem},
		{"addmul", 2, b_addmul},   {"shifts", 2, b_shifts},
		{"divmod", 2, b_divmod},   {"udivmod", 2, b_udivmod},
		{"cmp", 2, b_cmp},         {"cmpu", 2, b_cmpu},
		{"cmpx", 2, b_cmpx},       {"narrow", 2, b_narrow},
		{"ternary", 2, b_ternary}, {"land", 2, b_land},
		{"notneg", 2, b_notneg},
		{"ll-add", 2, b_ll_add},         {"ll-addsub", 2, b_ll_addsub},
		{"ll-mul", 2, b_ll_mul},         {"ll-umul", 2, b_ll_umul},
		{"ll-divraw", 2, b_ll_divraw},   {"ll-remraw", 2, b_ll_remraw},
		{"ll-divmod", 2, b_ll_divmod},   {"ll-udivmod", 2, b_ll_udivmod},
		{"ll-shiftraw", 2, b_ll_shiftraw}, {"ll-shifts", 2, b_ll_shifts},
		{"ll-ushifts", 2, b_ll_ushifts}, {"ll-cmp", 2, b_ll_cmp},
		{"ll-cmpu", 2, b_ll_cmpu},       {"ll-bits", 2, b_ll_bits},
		{"ll-notneg", 2, b_ll_notneg},   {"ll-narrow", 2, b_ll_narrow},
		{"ll-mix", 2, b_ll_mix},         {"ll-ternary", 2, b_ll_ternary},
		{"ll-land", 2, b_ll_land},       {"ll-bool", 2, b_ll_bool},
};

#define AST_NONE_U 0xFFFFFFFFu

typedef struct RawNode {
	int kind, op, type_t;
	long long ival;
	unsigned first_child, next_sib;
	unsigned long long type_ref, sym, fbits;
	unsigned bp, bs;
	int size;
	int etype;
} RawNode;

static int32_t *g_obj_ext;
static int *g_obj_ety;
static long g_obj_n;

static int spvgate_obj(AstArena *a, AstLocal n, int32_t *extent, int *etype) {
	(void)a;
	if (!g_obj_ext || (long)n >= g_obj_n)
		return 0;
	if (g_obj_ext[n] <= 0 || !g_obj_ety[n])
		return 0;
	*extent = g_obj_ext[n];
	*etype = g_obj_ety[n];
	return 1;
}

static void spvgate_obj_reset(long n) {
	free(g_obj_ext);
	free(g_obj_ety);
	g_obj_ext = n > 0 ? (int32_t *)calloc((size_t)n, sizeof *g_obj_ext) : NULL;
	g_obj_ety = n > 0 ? (int *)calloc((size_t)n, sizeof *g_obj_ety) : NULL;
	g_obj_n = (g_obj_ext && g_obj_ety) ? n : 0;
}

static AstArena *rebuild_arena(const RawNode *raw, int n, AstLocal *root_out,
															 long root_in) {
	AstArena *a = ast_arena_new();
	int i;
	for (i = 0; i < n; i++) {
		AstLocal id = ast_node(a, (uint16_t)raw[i].kind);
		if ((long)id != i) {
			ast_arena_free(a);
			return NULL;
		}
		ast_set_op(a, id, raw[i].op);
		ast_set_type(a, id, raw[i].type_t, (uint64_t)raw[i].type_ref);
		if (raw[i].bp || raw[i].bs)
			ast_set_type_bf(a, id, raw[i].type_t, (uint64_t)raw[i].type_ref,
											(int)raw[i].bp, (int)raw[i].bs);
		ast_set_ival(a, id, (uint64_t)raw[i].ival);
		if (raw[i].sym)
			ast_set_sym(a, id, (uint64_t)raw[i].sym);
		if (raw[i].fbits)
			ast_set_fbits(a, id, (uint64_t)raw[i].fbits);
	}
	for (i = 0; i < n; i++) {
		unsigned c = raw[i].first_child;
		while (c != AST_NONE_U && (int)c < n) {
			ast_add_child(a, (AstLocal)i, (AstLocal)c);
			c = raw[c].next_sib;
		}
	}
	*root_out = (AstLocal)root_in;
	return a;
}

static int live_type(AstArena *a, AstLocal n, int32_t want, int *out) {
	AstLocal c;
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_Ref) {
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM) &&
				(int32_t)(int64_t)ast_ival(a, n) == want) {
			*out = ast_type_t(a, n);
			return 1;
		}
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (live_type(a, c, want, out))
			return 1;
	return 0;
}

static void live_types(AstArena *a, AstLocal root, const int32_t *off, int n,
											 int *ty) {
	int k;
	for (k = 0; k < n; k++) {
		ty[k] = VT_INT;
		live_type(a, root, off[k], &ty[k]);
	}
}

static long subtree_nodes(AstArena *a, AstLocal n) {
	AstLocal c;
	long k = 1;
	if (n == AST_NONE)
		return 0;
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		k += subtree_nodes(a, c);
	return k;
}

static int trial_lower(AstArena *a, AstLocal n, const int32_t *off, int nlive) {
	GateMod m;
	uint32_t base;
	GateV val;
	int ok;
	gate_module_begin(&m, nlive);
	base = gate_main_begin(&m, nlive);
	ok = gate_expr(&m, a, n, off, nlive, base, &val) && !m.failed;
#if !MCC_GPU_LANG_MSL
	if (ok && m.used_f64 && !g_f64) {
		g_f64_skipped++;
		ok = 0;
	}
#endif
	gate_module_free(&m);
	return ok;
}

static const int RUNGS[] = {1, 2, 4, 8, 16};

static int32_t *g_in, *g_gout;
static int64_t *g_cout;
static unsigned char *g_def;
static long tot_pts, tot_cmp, tot_vac, tot_bad, tot_reject;
static long tot_idx_cmp, tot_idx_vac;
static int in_idx_slice;

static int mutate;

static int64_t fit_rung_v(int64_t pat, int w);

static long run_one_slice(AstArena *a, AstLocal root, const int32_t *off,
													int nlive, const char *label, int quiet) {
	long bad_all = 0;
	int r;
	for (r = 0; r < (int)(sizeof RUNGS / sizeof RUNGS[0]); r++) {
		int w = RUNGS[r], t, k, ntuple, nwords;
		/* 64-bit, not long: long is 32 bits on LLP64, and the widest rung is
		 * 2^(16*nlive), which overflows a 32-bit span to 0 for nlive >= 2. */
		int64_t span = 1;
		GateMod m;
		GateUnit *code;
		uint32_t base;
		GateV val;
		int ltype[MAX_LIVE];
		long bad = 0, cmp = 0, vac = 0;

		for (k = 0; k < nlive; k++) {
			span *= ((int64_t)1 << w);
			if (span > MAX_TUPLES)
				break;
		}
		if (span > MAX_TUPLES || span <= 0)
			continue;
		ntuple = (int)span;

		live_types(a, root, off, nlive, ltype);
		for (t = 0; t < ntuple; t++) {
			long v = t;
			for (k = 0; k < nlive; k++) {
				put_in(g_in, (long)t * nlive + k,
							 ast_eval_slice_fit(fit_rung_v(v & ((1L << w) - 1), w),
																	ltype[k]));
				v >>= w;
			}
		}

		gate_module_begin(&m, nlive);
		base = gate_main_begin(&m, nlive);
		if (!gate_expr(&m, a, root, off, nlive, base, &val) || m.failed) {
			gate_module_free(&m);
			continue;
		}
#if !MCC_GPU_LANG_MSL
		if (m.used_f64 && !g_f64) {
			g_f64_skipped++;
			gate_module_free(&m);
			continue;
		}
#endif
		if (mutate)
			val = gate_mutate(&m, val);
		gate_main_end(&m, m.lane, val);
		code = gate_module_finish(&m, &nwords);

		for (t = 0; t < ntuple; t++) {
			int64_t vals[MAX_LIVE], o;
			for (k = 0; k < nlive; k++)
				vals[k] = get_in(g_in, (long)t * nlive + k);
			ast_eval_slice_undef = 0;
			g_def[t] = (unsigned char)(ast_eval_slice(a, root, off, vals, nlive, &o) &&
																 !ast_eval_slice_undef);
			g_cout[t] = g_def[t] ? o : 0;
		}

		if (gpu_run(code, nwords, g_in, ntuple, nlive, g_gout) != 0) {
			tot_reject++;
			free(code);
			gate_module_free(&m);
			continue;
		}
		for (t = 0; t < ntuple; t++) {
			int gdef = get_def(g_gout, t);
			if ((int)g_def[t] != gdef) {
				if (!bad && !quiet)
					printf("  DEFINEDNESS %s w=%d in0=%lld cpu=%d gpu=%d\n", label, w,
								 (long long)get_in(g_in, (long)t * nlive), (int)g_def[t], gdef);
				bad++;
				continue;
			}
			if (!g_def[t]) {
				vac++;
				continue;
			}
			cmp++;
			if (g_cout[t] != get_out(g_gout, t)) {
				if (!bad && !quiet)
					printf("  MISMATCH %s w=%d in0=%lld cpu=%lld gpu=%lld\n", label, w,
								 (long long)get_in(g_in, (long)t * nlive),
								 (long long)g_cout[t], (long long)get_out(g_gout, t));
				bad++;
			}
		}
		tot_pts += ntuple;
		tot_cmp += cmp;
		tot_vac += vac;
		tot_bad += bad;
		if (in_idx_slice) {
			tot_idx_cmp += cmp;
			tot_idx_vac += vac;
		}
		bad_all += bad;
		free(code);
		gate_module_free(&m);
	}
	return bad_all;
}

static long g_slices, g_bodies, g_lowerable_bodies, g_idx_slices;

static int subtree_has_dynidx(AstArena *a, AstLocal n) {
	AstLocal c;
	AstEvalSliceIdx ix;
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_Load &&
			ast_eval_slice_dynidx(a, ast_first_child(a, n), &ix))
		return 1;
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (subtree_has_dynidx(a, c))
			return 1;
	return 0;
}

static void scan_subtree(AstArena *a, AstLocal n, const char *fn, int minnodes,
												 int quiet, long limit) {
	AstLocal c;
	int32_t off[MAX_LIVE];
	int non = 0;
	if (n == AST_NONE || (limit && g_slices >= limit))
		return;
	if (subtree_nodes(a, n) >= minnodes &&
			ast_eval_slice_livein(a, n, off, &non, MAX_LIVE) &&
			non >= 1 && trial_lower(a, n, off, non)) {
		char label[160];
		snprintf(label, sizeof label, "%s#%ld/%dlive", fn, (long)n, non);
		g_slices++;
		in_idx_slice = subtree_has_dynidx(a, n);
		g_idx_slices += in_idx_slice;
		run_one_slice(a, n, off, non, label, quiet);
		in_idx_slice = 0;
		return;
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		scan_subtree(a, c, fn, minnodes, quiet, limit);
}

static int arena_mode(const char *path, int minnodes, long limit, int quiet) {
	FILE *f = fopen(path, "r");
	char line[512];
	RawNode *raw = NULL;
	int cap = 0;
	if (!f) {
		printf(GATE_NAME ": cannot open %s\n", path);
		return 1;
	}
	while (fgets(line, sizeof line, f)) {
		char fn[128];
		long n, root;
		int i;
		if (sscanf(line, "[arena] fn=%127s n=%ld root=%ld", fn, &n, &root) != 3)
			continue;
		if (n <= 0 || n > (1 << 22))
			continue;
		if (n > cap) {
			cap = (int)n;
			raw = realloc(raw, (size_t)cap * sizeof *raw);
		}
		for (i = 0; i < n; i++) {
			long id, fc, ns;
			if (!fgets(line, sizeof line, f))
				break;
			int nf = sscanf(line,
											"%ld %d %d %d %lld %ld %ld %llu %u %u %llu %llu %d %d",
											&id, &raw[i].kind, &raw[i].op, &raw[i].type_t,
											&raw[i].ival, &fc, &ns, &raw[i].type_ref, &raw[i].bp,
											&raw[i].bs, &raw[i].sym, &raw[i].fbits, &raw[i].size,
											&raw[i].etype);
			if (nf < 7)
				break;
			if (nf < 12) {
				raw[i].type_ref = 0;
				raw[i].bp = raw[i].bs = 0;
				raw[i].sym = raw[i].fbits = 0;
			}
			if (nf < 13)
				raw[i].size = 0;
			if (nf < 14)
				raw[i].etype = 0;
			raw[i].first_child = (unsigned)fc;
			raw[i].next_sib = (unsigned)ns;
		}
		if (i != n)
			continue;
		{
			AstLocal rt;
			AstArena *a = rebuild_arena(raw, (int)n, &rt, root);
			long before = g_slices;
			if (!a)
				continue;
			g_bodies++;
			spvgate_obj_reset(n);
			for (i = 0; i < n && i < g_obj_n; i++) {
				g_obj_ext[i] = (int32_t)raw[i].size;
				g_obj_ety[i] = raw[i].etype;
			}
			scan_subtree(a, rt, fn, minnodes, quiet, limit);
			spvgate_obj_reset(0);
			if (g_slices > before)
				g_lowerable_bodies++;
			ast_arena_free(a);
		}
		if (limit && g_slices >= limit)
			break;
	}
	fclose(f);
	free(raw);
	printf(GATE_NAME ": arenas=%ld bodies-with-lowerable-slice=%ld slices=%ld "
									 "runtime-idx=%ld runtime-idx-compared=%ld "
									 "runtime-idx-poisoned=%ld\n",
				 g_bodies, g_lowerable_bodies, g_slices, g_idx_slices, tot_idx_cmp,
				 tot_idx_vac);
	printf(GATE_NAME ": dispatches=%ld lanes=%ld points=%ld compared=%ld vacuous=%ld "
				 "mismatches=%ld rejected-modules=%ld fp64-unsupported=%ld\n",
				 g_dispatches, g_lanes, tot_pts, tot_cmp, tot_vac, tot_bad, tot_reject,
				 g_f64_skipped);
	if (!g_dispatches) {
		printf(GATE_NAME ": FAIL (no GPU dispatch happened)\n");
		return 1;
	}
	printf(GATE_NAME ": %s\n", (tot_bad || tot_reject) ? "FAIL" : "OK");
	return (tot_bad || tot_reject) ? 1 : 0;
}

static int64_t fit_rung(int64_t pat, int w) {
	if (w >= 64)
		return pat;
	int64_t m = (int64_t)1 << (w - 1);
	return (pat ^ m) - m;
}

static int64_t fit_rung_v(int64_t pat, int w) { return fit_rung(pat, w); }

static int corrupt_at = -1;

#define MEM_LANES 64

#if MCC_GPU_LANG_MSL
static int mem_case(void) {
	printf(GATE_NAME ": the binding-2 case has no Metal arm\n");
	return 77;
}
#else
static int mem_case(void) {
	SpvMod m;
	SpvRegion r;
	SpvV v, p;
	uint32_t base;
	uint32_t *code = NULL;
	int nw = 0, t, bad = 0;
	unsigned long msz = 0;
	unsigned char *win = (unsigned char *)mem_window(&msz);
	int32_t *in = (int32_t *)calloc((size_t)MEM_LANES * MCC_GPU_IN_SLOTS,
																	sizeof *in);
	int32_t *ob = (int32_t *)calloc((size_t)MEM_LANES * MCC_GPU_OUT_SLOTS,
																	sizeof *ob);
	int64_t want[MEM_LANES];

	if (!win || !in || !ob) {
		printf(GATE_NAME ": mem case out of memory\n");
		free(in);
		free(ob);
		return 1;
	}
	spv_module_begin(&m, 1);
	m.mem_base = (int64_t)(intptr_t)win;
	m.mem_nbyte = (uint32_t)msz;
	base = spv_main_begin(&m, 1);
	if (!spv_mem_region(&m, &r)) {
		printf(GATE_NAME ": mem case could not open the binding-2 region\n");
		spv_module_free(&m);
		free(in);
		free(ob);
		return 1;
	}
	p = spv_load_live_v(&m, base, 0, 1, 1);
	v = spv_load_region(&m, &r, spv_mem_off(&m, p), VT_LLONG);
	if (mutate)
		v = spv_mutate(&m, v);
	spv_main_end(&m, m.lane, v);
	code = spv_module_finish(&m, &nw);
	spv_module_free(&m);
	if (!code || nw <= 0) {
		printf(GATE_NAME ": mem case emitted no module\n");
		free(code);
		free(in);
		free(ob);
		return 1;
	}
	if (corrupt_at >= 0 && corrupt_at < nw)
		code[corrupt_at] ^= 1u;

	for (t = 0; t < MEM_LANES; t++) {
		int64_t a = (int64_t)(intptr_t)(win + (size_t)t * 8);
		want[t] = (int64_t)(0x0123456789ABCDEFLL ^ ((int64_t)(t + 1) * 0x9E3779B9LL));
		memcpy(win + (size_t)t * 8, &want[t], 8);
		put_in(in, t, a);
	}
	if (gpu_run(code, nw, in, MEM_LANES, 1, ob) != 0) {
		printf(GATE_NAME ": mem case GPU REJECTED MODULE\n");
		free(code);
		free(in);
		free(ob);
		return 1;
	}
	for (t = 0; t < MEM_LANES; t++) {
		if (!get_def(ob, t)) {
			if (bad < 4)
				printf("  mem lane %d UNDEFINED (the address fell outside binding 2)\n",
							 t);
			bad++;
			continue;
		}
		if (get_out(ob, t) != want[t]) {
			if (bad < 4)
				printf("  mem lane %d MISMATCH want=%lld got=%lld\n", t,
							 (long long)want[t], (long long)get_out(ob, t));
			bad++;
		}
	}
	free(code);
	free(in);
	free(ob);
	printf(GATE_NAME ": mem lanes=%d through binding 2, %d bad\n", MEM_LANES,
				 bad);
	printf(GATE_NAME ": %s\n", bad ? "FAIL" : "OK");
	return bad ? 1 : 0;
}
#endif

int main(int argc, char **argv) {
	int only = -1, i, r, ci;
	long total_pts = 0, total_cmp = 0, total_vac = 0, mismatch = 0;
	const char *arenas = NULL;
	const char *emit_only = NULL;
	long emitted = 0;
	int do_mem = 0;
	int minnodes = 3, quiet = 0;
	long limit = 0;
	int32_t *in =
			malloc(sizeof(int32_t) * MAX_TUPLES * MAX_LIVE * MCC_GPU_IN_SLOTS);
	int32_t *gout = malloc(sizeof(int32_t) * MAX_TUPLES * MCC_GPU_OUT_SLOTS);
	int64_t *cout = malloc(sizeof(int64_t) * MAX_TUPLES);
	unsigned char *defined = malloc(MAX_TUPLES);
	g_in = in;
	g_gout = gout;
	g_cout = cout;
	g_def = defined;
	ast_eval_slice_obj_fn = spvgate_obj;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--corrupt") && i + 1 < argc)
			corrupt_at = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--mutate"))
			mutate = 1;
		else if (!strcmp(argv[i], "--arenas") && i + 1 < argc)
			arenas = argv[++i];
		else if (!strcmp(argv[i], "--emit-only") && i + 1 < argc)
			emit_only = argv[++i];
		else if (!strcmp(argv[i], "--mem"))
			do_mem = 1;
		else if (!strcmp(argv[i], "--min-nodes") && i + 1 < argc)
			minnodes = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--limit") && i + 1 < argc)
			limit = atol(argv[++i]);
		else if (!strcmp(argv[i], "--quiet"))
			quiet = 1;
		else
			only = atoi(argv[i]);
	}

	if (!emit_only)
		gpu_init();
	{
		int k;
		for (k = 1; k < argc; k++)
			if (!strcmp(argv[k], "--spv") && k + 1 < argc) {
				FILE *fp = fopen(argv[k + 1], "rb");
				GateUnit *buf;
				long sz;
				int rc2;
				if (!fp) { printf(GATE_NAME ": cannot open %s\n", argv[k + 1]); return 1; }
				fseek(fp, 0, SEEK_END); sz = ftell(fp); fseek(fp, 0, SEEK_SET);
				buf = malloc((size_t)sz);
				if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) { printf("short read\n"); return 1; }
				fclose(fp);
				memset(in, 0, sizeof(int32_t) * 64);
				printf(GATE_NAME ": dispatching %s (%ld bytes) on %s\n", argv[k + 1], sz,
							 g_devname);
				rc2 = gpu_run(buf, (int)(sz / MCC_GPU_CODE_UNIT), in, 64, 1, gout);
				printf(GATE_NAME ": rc=%d out0=%lld def0=%d\n", rc2,
							 (long long)get_out(gout, 0), get_def(gout, 0));
				return rc2 == 0 ? 0 : 1;
			}
	}
	if (do_mem) {
		printf(GATE_NAME ": device %s\n", g_devname);
		return mem_case();
	}
	if (arenas) {
		printf(GATE_NAME ": device %s\n", g_devname);
		printf(GATE_NAME ": real slices from %s (min-nodes=%d)\n", arenas, minnodes);
		return arena_mode(arenas, minnodes, limit, quiet);
	}
	if (emit_only)
		printf(GATE_NAME ": emit-only, no device, modules into %s\n", emit_only);
	else
		printf(GATE_NAME ": device %s\n", g_devname);
	printf(GATE_NAME ": %d cases, rungs 1/2/4/8/16 bits, exhaustive per rung\n",
				 (int)(sizeof CASES / sizeof CASES[0]));

	for (ci = 0; ci < (int)(sizeof CASES / sizeof CASES[0]); ci++) {
		const Case *c = &CASES[ci];
		int case_bad = 0;
		long case_cmp = 0;
		if (only >= 0 && ci != only)
			continue;
		for (r = 0; r < (int)(sizeof RUNGS / sizeof RUNGS[0]); r++) {
			int w = RUNGS[r];
			int off[MAX_LIVE];
			AstArena *a;
			AstLocal root;
			GateMod m;
			GateUnit *code;
			uint32_t base;
			GateV val;
			int nwords, k, t, ntuple;
			/* Same span arithmetic, same guards, as the sweep above -- 64-bit so
			 * the widest rung does not wrap to 0 on LLP64, and the <= 0 test so a
			 * wrap can never reach gpu_run as an empty dispatch. A zero-tuple
			 * dispatch asks vkAllocateMemory for 0 bytes, which is invalid and
			 * which NVIDIA reports as VK_ERROR_OUT_OF_DEVICE_MEMORY. */
			int64_t span = 1;
			int ltype[MAX_LIVE];

			for (k = 0; k < c->nlive; k++)
				off[k] = -8 * (k + 1);
			for (k = 0; k < c->nlive; k++) {
				span *= ((int64_t)1 << w);
				if (span > MAX_TUPLES)
					break;
			}
			if (span > MAX_TUPLES || span <= 0)
				continue;
			ntuple = (int)span;

			a = ast_arena_new();
			root = c->build(a, off);
			for (k = 0; k < c->nlive; k++) {
				ltype[k] = VT_INT;
				live_type(a, root, (int32_t)off[k], &ltype[k]);
			}
			if (!emit_only)
				for (t = 0; t < ntuple; t++) {
					long v = t;
					for (k = 0; k < c->nlive; k++) {
						put_in(in, (long)t * c->nlive + k,
									 ast_eval_slice_fit(fit_rung(v & ((1L << w) - 1), w),
																			ltype[k]));
						v >>= w;
					}
				}

			gate_module_begin(&m, c->nlive);
			base = gate_main_begin(&m, c->nlive);
			if (!gate_expr(&m, a, root, off, c->nlive, base, &val)) {
				printf("  %-10s w=%-2d SKIP (not lowerable)\n", c->name, w);
				gate_module_free(&m);
				ast_arena_free(a);
				continue;
			}
			if (mutate)
				val = gate_mutate(&m, val);
			gate_main_end(&m, m.lane, val);
			code = gate_module_finish(&m, &nwords);
			if (corrupt_at >= 0 && corrupt_at < nwords)
				code[corrupt_at] = (GateUnit)(code[corrupt_at] ^ 1u);
			{
				const char *dp = emit_only ? emit_only : getenv("SPVGATE_DUMP");
				if (dp) {
					char path[512];
					FILE *fp;
					snprintf(path, sizeof path, "%s/%s_w%d." MCC_GPU_CODE_SUFFIX, dp,
									 c->name, w);
					fp = fopen(path, "wb");
					if (!fp) {
						printf(GATE_NAME ": cannot write %s\n", path);
						return 1;
					}
					if (fwrite(code, MCC_GPU_CODE_UNIT, (size_t)nwords, fp) !=
							(size_t)nwords) {
						printf(GATE_NAME ": short write %s\n", path);
						fclose(fp);
						return 1;
					}
					fclose(fp);
					emitted++;
				}
			}
			if (emit_only) {
				case_cmp++;
				free(code);
				gate_module_free(&m);
				ast_arena_free(a);
				continue;
			}

			for (t = 0; t < ntuple; t++) {
				int64_t vals[MAX_LIVE];
				int64_t o;
				for (k = 0; k < c->nlive; k++)
					vals[k] = get_in(in, (long)t * c->nlive + k);
				ast_eval_slice_undef = 0;
				defined[t] = (unsigned char)(ast_eval_slice(a, root, off, vals,
																									 c->nlive, &o) &&
																		 !ast_eval_slice_undef);
				cout[t] = defined[t] ? o : 0;
			}

			if (gpu_run(code, nwords, in, ntuple, c->nlive, gout) != 0) {
				printf("  %-10s w=%-2d GPU REJECTED MODULE\n", c->name, w);
				case_bad = 1;
				free(code);
				gate_module_free(&m);
				ast_arena_free(a);
				continue;
			}

			long bad = 0, cmp = 0, vac = 0;
			for (t = 0; t < ntuple; t++) {
				int gdef = get_def(gout, t);
				if ((int)defined[t] != gdef) {
					if (bad == 0 && mismatch < 8)
						printf("  %-10s w=%-2d DEFINEDNESS t=%d in=[%lld,%lld] cpu=%d "
									 "gpu=%d ntuple=%d\n",
									 c->name, w, t,
									 (long long)get_in(in, (long)t * c->nlive),
									 (long long)get_in(in, (long)t * c->nlive + 1),
									 (int)defined[t], gdef, ntuple);
					bad++;
					continue;
				}
				if (!defined[t]) {
					vac++;
					continue;
				}
				cmp++;
				if (cout[t] != get_out(gout, t)) {
					if (bad == 0 && mismatch < 8)
						printf("  %-10s w=%-2d MISMATCH in=[%lld,%lld] cpu=%lld "
									 "gpu=%lld\n",
									 c->name, w, (long long)get_in(in, (long)t * c->nlive),
									 (long long)get_in(in, (long)t * c->nlive + 1),
									 (long long)cout[t], (long long)get_out(gout, t));
					bad++;
				}
			}
			total_pts += ntuple;
			total_cmp += cmp;
			total_vac += vac;
			case_cmp += cmp;
			mismatch += bad;
			if (bad)
				case_bad = 1;
			free(code);
			gate_module_free(&m);
			ast_arena_free(a);
		}
		/* A case that lowered nothing, or that compared only vacuous points,
		 * used to print OK -- so "0 mismatches" was indistinguishable from
		 * "0 points compared", and a case that silently stopped lowering read as
		 * evidence. It is a failure: this case proves nothing. */
		if (!case_bad && case_cmp == 0) {
			printf("  %-10s FAIL (0 defined points compared -- proves nothing)\n",
						 c->name);
			case_bad = 1;
			mismatch++;
		} else {
			printf("  %-10s %s (%ld points)\n", c->name, case_bad ? "FAIL" : "OK",
						 case_cmp);
		}
	}

	if (emit_only) {
		printf(GATE_NAME ": emitted=%ld\n", emitted);
		if (emitted == 0) {
			printf(GATE_NAME ": FAIL (no module was emitted)\n");
			return 1;
		}
		return 0;
	}
	printf(GATE_NAME ": dispatches=%ld lanes=%ld points=%ld compared=%ld vacuous=%ld "
				 "mismatches=%ld\n",
				 g_dispatches, g_lanes, total_pts, total_cmp, total_vac, mismatch);
	if (g_dispatches == 0) {
		printf(GATE_NAME ": FAIL (no GPU dispatch happened)\n");
		return 1;
	}
	printf(GATE_NAME ": %s\n", mismatch ? "FAIL" : "OK");
	return mismatch ? 1 : 0;
}
