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

#include "ast_eval_slice.h"

#undef malloc
#undef realloc
#undef free
#undef strdup

#include "mccspv.h"

#ifndef AST_EVAL_SLICE_PROVIDED
#error "spvgate needs the real ast_eval_slice; the mccast.c fallback stub returns 1 without writing *out, so every comparison would read uninitialised memory and pass."
#endif

#include <stdio.h>
#include <string.h>
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

#define MAX_LIVE 4
#define MAX_TUPLES (1 << 18)

static VkInstance g_inst;
static VkPhysicalDevice g_phys;
static VkDevice g_dev;
static VkQueue g_q;
static unsigned g_qfam;
static char g_devname[256];
static long g_dispatches;
static long g_lanes;

static void gpu_init(void) {
	VkApplicationInfo ai;
	VkInstanceCreateInfo ici;
	VkPhysicalDevice devs[8];
	VkQueueFamilyProperties qf[32];
	VkPhysicalDeviceProperties props;
	VkDeviceQueueCreateInfo qci;
	VkDeviceCreateInfo dci;
	float prio = 1.0f;
	unsigned ndev = 8, nq = 32, i;

	memset(&ai, 0, sizeof ai);
	ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	ai.pApplicationName = "spvgate";
	ai.apiVersion = VK_API_VERSION_1_1;
	memset(&ici, 0, sizeof ici);
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.pApplicationInfo = &ai;
	VK(vkCreateInstance(&ici, 0, &g_inst));
	VK(vkEnumeratePhysicalDevices(g_inst, &ndev, devs));
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
	VK(vkCreateDevice(g_phys, &dci, 0, &g_dev));
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

static int gpu_run(const uint32_t *code, int nwords, const int32_t *in,
									 int ntuple, int nlive, int32_t *out) {
	VkBuffer bin, bout;
	VkDeviceMemory min_, mout;
	void *pin, *pout;
	VkDescriptorSetLayoutBinding dslb[2];
	VkDescriptorSetLayoutCreateInfo dslci;
	VkDescriptorSetLayout dsl;
	VkDescriptorPoolSize dps;
	VkDescriptorPoolCreateInfo dpci;
	VkDescriptorPool dpool;
	VkDescriptorSetAllocateInfo dsai;
	VkDescriptorSet dset;
	VkDescriptorBufferInfo dbi[2];
	VkWriteDescriptorSet wds[2];
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
	make_buffer((VkDeviceSize)cap * nlive * 4, &bin, &min_, &pin);
	make_buffer((VkDeviceSize)cap * 4, &bout, &mout, &pout);
	memset(pin, 0, (size_t)cap * nlive * 4);
	memcpy(pin, in, (size_t)ntuple * nlive * 4);
	memset(pout, 0, (size_t)cap * 4);

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
	VK(vkCreateDescriptorSetLayout(g_dev, &dslci, 0, &dsl));

	memset(&dps, 0, sizeof dps);
	dps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	dps.descriptorCount = 2;
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
	memset(wds, 0, sizeof wds);
	for (i = 0; i < 2; i++) {
		wds[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		wds[i].dstSet = dset;
		wds[i].dstBinding = (unsigned)i;
		wds[i].descriptorCount = 1;
		wds[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		wds[i].pBufferInfo = &dbi[i];
	}
	vkUpdateDescriptorSets(g_dev, 2, wds, 0, 0);

	memset(&smci, 0, sizeof smci);
	smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	smci.codeSize = (size_t)nwords * 4;
	smci.pCode = code;
	rc = vkCreateShaderModule(g_dev, &smci, 0, &sm);
	if (rc != VK_SUCCESS)
		return -1;

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
	if (rc != VK_SUCCESS)
		return -1;

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
	memcpy(out, pout, (size_t)ntuple * 4);
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
								mk_bin(a, '<', mk_ref(a, o[0], VT_INT), mk_ref(a, o[1], VT_INT),
											 VT_INT),
								mk_bin(a, TOK_EQ, mk_ref(a, o[0], VT_INT),
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

static const Case CASES[] = {
		{"sdiv", 2, b_sdiv},       {"srem", 2, b_srem},
		{"addmul", 2, b_addmul},   {"shifts", 2, b_shifts},
		{"divmod", 2, b_divmod},   {"udivmod", 2, b_udivmod},
		{"cmp", 2, b_cmp},         {"narrow", 2, b_narrow},
		{"ternary", 2, b_ternary}, {"land", 2, b_land},
		{"notneg", 2, b_notneg},
};

static const int RUNGS[] = {1, 2, 4, 8, 16};

static int64_t fit_rung(int64_t pat, int w) {
	if (w >= 64)
		return pat;
	int64_t m = (int64_t)1 << (w - 1);
	return (pat ^ m) - m;
}

static int corrupt_at = -1;
static int mutate = 0;

static int spv_mutate_module(uint32_t *w, int nwords) {
	int i = 5, hits = 0;
	while (i < nwords) {
		int wc = (int)(w[i] >> 16);
		int op = (int)(w[i] & 0xFFFFu);
		if (wc <= 0)
			break;
		if (op == SpvOpIAdd) {
			w[i] = ((uint32_t)wc << 16) | (uint32_t)SpvOpISub;
			hits++;
		}
		i += wc;
	}
	return hits;
}

int main(int argc, char **argv) {
	int only = -1, i, r, ci;
	long total_pts = 0, total_cmp = 0, total_vac = 0, mismatch = 0;
	int32_t *in = malloc(sizeof(int32_t) * MAX_TUPLES * MAX_LIVE);
	int32_t *gout = malloc(sizeof(int32_t) * MAX_TUPLES);
	int64_t *cout = malloc(sizeof(int64_t) * MAX_TUPLES);
	unsigned char *defined = malloc(MAX_TUPLES);

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--corrupt") && i + 1 < argc)
			corrupt_at = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--mutate"))
			mutate = 1;
		else
			only = atoi(argv[i]);
	}

	gpu_init();
	printf("spvgate: device %s\n", g_devname);
	printf("spvgate: %d cases, rungs 1/2/4/8/16 bits, exhaustive per rung\n",
				 (int)(sizeof CASES / sizeof CASES[0]));

	for (ci = 0; ci < (int)(sizeof CASES / sizeof CASES[0]); ci++) {
		const Case *c = &CASES[ci];
		int case_bad = 0;
		if (only >= 0 && ci != only)
			continue;
		for (r = 0; r < (int)(sizeof RUNGS / sizeof RUNGS[0]); r++) {
			int w = RUNGS[r];
			int off[MAX_LIVE];
			AstArena *a;
			AstLocal root;
			SpvMod m;
			uint32_t *code, base;
			int nwords, k, t, ntuple;
			long span = 1;

			for (k = 0; k < c->nlive; k++)
				off[k] = -8 * (k + 1);
			for (k = 0; k < c->nlive; k++)
				span *= (1L << w);
			if (span > MAX_TUPLES)
				continue;
			ntuple = (int)span;

			for (t = 0; t < ntuple; t++) {
				long v = t;
				for (k = 0; k < c->nlive; k++) {
					in[(long)t * c->nlive + k] = (int32_t)fit_rung(v & ((1L << w) - 1), w);
					v >>= w;
				}
			}

			a = ast_arena_new();
			root = c->build(a, off);
			spv_module_begin(&m, c->nlive);
			base = spv_main_begin(&m, c->nlive);
			uint32_t val;
			if (!spv_expr(&m, a, root, off, c->nlive, base, &val)) {
				printf("  %-8s w=%-2d SKIP (not lowerable)\n", c->name, w);
				spv_module_free(&m);
				ast_arena_free(a);
				continue;
			}
			uint32_t lane =
					spv_emit3(&m, SpvOpSDiv, m.id_int, base, spv_const(&m, c->nlive));
			spv_main_end(&m, lane, val);
			code = spv_module_finish(&m, &nwords);
			if (corrupt_at >= 0 && corrupt_at < nwords)
				code[corrupt_at] ^= 1u;
			if (mutate)
				spv_mutate_module(code, nwords);
			{
				const char *dp = getenv("SPVGATE_DUMP");
				if (dp) {
					char path[512];
					FILE *fp;
					snprintf(path, sizeof path, "%s/%s_w%d.spv", dp, c->name, w);
					fp = fopen(path, "wb");
					if (fp) {
						fwrite(code, 4, (size_t)nwords, fp);
						fclose(fp);
					}
				}
			}

			for (t = 0; t < ntuple; t++) {
				int64_t vals[MAX_LIVE];
				int64_t o;
				for (k = 0; k < c->nlive; k++)
					vals[k] = in[(long)t * c->nlive + k];
				defined[t] = (unsigned char)ast_eval_slice(a, root, off, vals,
																									c->nlive, &o);
				cout[t] = defined[t] ? o : 0;
			}

			if (gpu_run(code, nwords, in, ntuple, c->nlive, gout) != 0) {
				printf("  %-8s w=%-2d GPU REJECTED MODULE\n", c->name, w);
				case_bad = 1;
				free(code);
				spv_module_free(&m);
				ast_arena_free(a);
				continue;
			}

			long bad = 0, cmp = 0, vac = 0;
			for (t = 0; t < ntuple; t++) {
				if (!defined[t]) {
					vac++;
					continue;
				}
				cmp++;
				if ((int32_t)cout[t] != gout[t]) {
					if (bad == 0 && mismatch < 8)
						printf("  %-8s w=%-2d MISMATCH in=[%d,%d] cpu=%d gpu=%d\n", c->name,
									 w, in[(long)t * c->nlive], in[(long)t * c->nlive + 1],
									 (int)cout[t], gout[t]);
					bad++;
				}
			}
			total_pts += ntuple;
			total_cmp += cmp;
			total_vac += vac;
			mismatch += bad;
			if (bad)
				case_bad = 1;
			free(code);
			spv_module_free(&m);
			ast_arena_free(a);
		}
		printf("  %-8s %s\n", c->name, case_bad ? "FAIL" : "OK");
	}

	printf("spvgate: dispatches=%ld lanes=%ld points=%ld compared=%ld vacuous=%ld "
				 "mismatches=%ld\n",
				 g_dispatches, g_lanes, total_pts, total_cmp, total_vac, mismatch);
	if (g_dispatches == 0) {
		printf("spvgate: FAIL (no GPU dispatch happened)\n");
		return 1;
	}
	printf("spvgate: %s\n", mismatch ? "FAIL" : "OK");
	return mismatch ? 1 : 0;
}
