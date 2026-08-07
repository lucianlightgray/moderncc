#ifndef MCC_GPU_PROVIDED
#define MCC_GPU_PROVIDED 1

#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#if defined(__unix__) || defined(__APPLE__)
#include <pthread.h>
static pthread_mutex_t mcc_gpu_lock = PTHREAD_MUTEX_INITIALIZER;
#define MCC_GPU_LOCK() pthread_mutex_lock(&mcc_gpu_lock)
#define MCC_GPU_UNLOCK() pthread_mutex_unlock(&mcc_gpu_lock)
#else
#define MCC_GPU_LOCK() ((void)0)
#define MCC_GPU_UNLOCK() ((void)0)
#endif

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
static int mcc_gpu_closing;

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

static void mcc_gpu_quiesce(void) {
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

static int mcc_gpu_run_locked(const uint32_t *code, int nwords,
															const int32_t *in, int ntuple, int nlive,
															int32_t *out);

static int mcc_gpu_run(const uint32_t *code, int nwords, const int32_t *in,
											 int ntuple, int nlive, int32_t *out) {
	int rc;
	MCC_GPU_LOCK();
	rc = mcc_gpu_closing
					 ? 0
					 : mcc_gpu_run_locked(code, nwords, in, ntuple, nlive, out);
	MCC_GPU_UNLOCK();
	return rc;
}

static int mcc_gpu_run_locked(const uint32_t *code, int nwords,
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
	int cap = ((ntuple + SPV_LOCAL_SIZE - 1) / SPV_LOCAL_SIZE) * SPV_LOCAL_SIZE;

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
	vkCmdDispatch(cb, (unsigned)(cap / SPV_LOCAL_SIZE), 1, 1);
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

#endif
