#ifndef _MCC_GPU_OFFLOAD_H
#define _MCC_GPU_OFFLOAD_H

/*
 * mcc_gpu_offload.h -- minimal Vulkan compute-offload runtime for compiled mcc
 * programs (T-lin-10526). Distilled from the tests/gpu/*_vk.c host template into
 * a reusable primitive so a data-parallel loop can run part of its range on the
 * GPU while the CPU thread pool (native pthreads or the coop backend) runs the
 * rest -- heterogeneous CPU+GPU execution.
 *
 * Contract:
 *   - One host-visible/coherent SSBO at binding 0 (persistently mapped: upload
 *     and readback are plain memcpy on the returned pointer).
 *   - Per-dispatch scalars via a push-constant block (opaque bytes + size).
 *   - A fixed workgroup grid + an in-shader grid-stride loop cover any range.
 *   - Device chosen by score (discrete > integrated), shaderFloat64 REQUIRED
 *     (kernels use double for CPU-parity), overridable by env MCC_GPU_DEVICE
 *     (0-based enumeration index, or a case-insensitive substring of the name).
 *   - EVERY entry point reports failure by return value and never aborts, so a
 *     host with no usable device falls back to its CPU path. This header links
 *     Vulkan directly (-lvulkan); build it only where a loader is present.
 *
 * This header is Vulkan-dependent by nature; it pulls <vulkan/vulkan.h> (which
 * brings only <stdint.h>/<stddef.h>) and hand-declares the handful of libc
 * symbols it needs, so it does not drag in colliding typedefs (mirrors the
 * discipline in mcc_coop_threads.h). Gate it with -DMCC_GPU_OFFLOAD.
 */

#include <vulkan/vulkan.h>

extern void *malloc(size_t);
extern void free(void *);
extern char *getenv(const char *);
extern int atoi(const char *);

#ifndef MCC_GPU_LOCAL_SIZE
#define MCC_GPU_LOCAL_SIZE 256u
#endif

typedef struct {
	VkInstance inst;
	VkPhysicalDevice phys;
	VkDevice dev;
	VkQueue queue;
	unsigned qfam;
	VkDeviceMemory mem;
	VkBuffer buf;
	void *map;
	unsigned long bufbytes;
	VkDescriptorSetLayout dsl;
	VkDescriptorPool dpool;
	VkDescriptorSet dset;
	VkShaderModule shader;
	VkPipelineLayout playout;
	VkPipeline pipe;
	VkCommandPool cpool;
	VkCommandBuffer cbuf;
	VkFence fence;
	int is_discrete; /* 1 = discrete GPU (throughput-weighting hint), 0 = other */
	char name[256];
} mcc_gpu;

/* case-insensitive "does hay contain needle" (tiny, avoids strcasestr) */
static int __mcc_gpu_ci_contains(const char *hay, const char *needle) {
	if (!needle || !needle[0])
		return 1;
	for (; *hay; hay++) {
		const char *h = hay, *n = needle;
		while (*h && *n) {
			int a = *h, b = *n;
			if (a >= 'A' && a <= 'Z')
				a += 32;
			if (b >= 'A' && b <= 'Z')
				b += 32;
			if (a != b)
				break;
			h++;
			n++;
		}
		if (!*n)
			return 1;
	}
	return 0;
}

static int __mcc_gpu_is_num(const char *s) {
	if (!s || !*s)
		return 0;
	for (; *s; s++)
		if (*s < '0' || *s > '9')
			return 0;
	return 1;
}

static int __mcc_gpu_score(const VkPhysicalDeviceProperties *p) {
	int s;
	switch (p->deviceType) {
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: s = 5000; break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: s = 4000; break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: s = 3000; break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU: s = 1000; break;
	default: s = 2000; break;
	}
	return s;
}

static int __mcc_gpu_make_instance(VkInstance *inst) {
	VkApplicationInfo ai;
	VkInstanceCreateInfo ci;
	unsigned j;
	for (j = 0; j < sizeof ai; j++) ((unsigned char *)&ai)[j] = 0;
	for (j = 0; j < sizeof ci; j++) ((unsigned char *)&ci)[j] = 0;
	ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	ai.pApplicationName = "mcc-gpu-offload";
	ai.apiVersion = VK_API_VERSION_1_1;
	ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ci.pApplicationInfo = &ai;
	return vkCreateInstance(&ci, 0, inst) == VK_SUCCESS ? 0 : 1;
}

/*
 * Fill out[] with the shaderFloat64-capable physical devices, ordered by score
 * descending (discrete > integrated), enumeration order breaking ties. Returns
 * the count (<= max). This is the stable device ordering that indexes both
 * mcc_gpu_num_devices() and mcc_gpu_open_nth(); the double kernels require fp64,
 * so devices lacking it are excluded from the ordering entirely.
 */
static int __mcc_gpu_list(VkInstance inst, VkPhysicalDevice *out, int max) {
	unsigned ndev = 16, i;
	VkPhysicalDevice devs[16];
	VkPhysicalDevice keep[16];
	int score[16], cnt = 0, a, b;
	if (vkEnumeratePhysicalDevices(inst, &ndev, devs) != VK_SUCCESS)
		return 0;
	for (i = 0; i < ndev && cnt < 16; i++) {
		VkPhysicalDeviceFeatures f;
		VkPhysicalDeviceProperties p;
		vkGetPhysicalDeviceFeatures(devs[i], &f);
		if (!f.shaderFloat64)
			continue;
		vkGetPhysicalDeviceProperties(devs[i], &p);
		keep[cnt] = devs[i];
		score[cnt] = __mcc_gpu_score(&p);
		cnt++;
	}
	for (a = 1; a < cnt; a++) {      /* stable insertion sort, score desc */
		VkPhysicalDevice kd = keep[a];
		int ks = score[a];
		b = a;
		while (b > 0 && score[b - 1] < ks) {
			keep[b] = keep[b - 1];
			score[b] = score[b - 1];
			b--;
		}
		keep[b] = kd;
		score[b] = ks;
	}
	if (cnt > max)
		cnt = max;
	for (a = 0; a < cnt; a++)
		out[a] = keep[a];
	return cnt;
}

/* Given g->inst and g->phys, create the compute queue + logical device (fp64
   enabled) and record the device name. Returns 0 on success. */
static int __mcc_gpu_finish(mcc_gpu *g) {
	unsigned i, j, nq = 32, found = 0;
	VkQueueFamilyProperties qf[32];
	VkPhysicalDeviceProperties p;
	VkDeviceQueueCreateInfo qci;
	VkDeviceCreateInfo dci;
	VkPhysicalDeviceFeatures want;
	float prio = 1.0f;
	vkGetPhysicalDeviceProperties(g->phys, &p);
	g->is_discrete = (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
	for (j = 0; j < 255 && p.deviceName[j]; j++)
		g->name[j] = p.deviceName[j];
	g->name[j] = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(g->phys, &nq, qf);
	for (i = 0; i < nq; i++)
		if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			g->qfam = i;
			found = 1;
			break;
		}
	if (!found)
		return 1;
	for (j = 0; j < sizeof qci; j++) ((unsigned char *)&qci)[j] = 0;
	for (j = 0; j < sizeof dci; j++) ((unsigned char *)&dci)[j] = 0;
	for (j = 0; j < sizeof want; j++) ((unsigned char *)&want)[j] = 0;
	want.shaderFloat64 = VK_TRUE;
	qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	qci.queueFamilyIndex = g->qfam;
	qci.queueCount = 1;
	qci.pQueuePriorities = &prio;
	dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.pEnabledFeatures = &want;
	if (vkCreateDevice(g->phys, &dci, 0, &g->dev) != VK_SUCCESS)
		return 1;
	vkGetDeviceQueue(g->dev, g->qfam, 0, &g->queue);
	return 0;
}

/* Number of shaderFloat64-capable devices (the count that indexes open_nth). */
static int mcc_gpu_num_devices(void) {
	VkInstance inst;
	VkPhysicalDevice list[16];
	int n;
	if (__mcc_gpu_make_instance(&inst))
		return 0;
	n = __mcc_gpu_list(inst, list, 16);
	vkDestroyInstance(inst, 0);
	return n;
}

/*
 * Open the nth device (0-based) in the score-descending fp64 ordering. Used to
 * drive several GPUs at once. Returns 0 on success (g populated), nonzero on
 * failure (g left zeroed). Each context owns its own VkInstance + device.
 */
static int mcc_gpu_open_nth(mcc_gpu *g, int nth) {
	VkPhysicalDevice list[16];
	unsigned i;
	int n;
	for (i = 0; i < sizeof *g; i++)
		((unsigned char *)g)[i] = 0;
	if (__mcc_gpu_make_instance(&g->inst))
		return 1;
	n = __mcc_gpu_list(g->inst, list, 16);
	if (nth < 0 || nth >= n)
		goto fail;
	g->phys = list[nth];
	if (__mcc_gpu_finish(g))
		goto fail;
	return 0;
fail:
	if (g->dev)
		vkDestroyDevice(g->dev, 0);
	if (g->inst)
		vkDestroyInstance(g->inst, 0);
	for (i = 0; i < sizeof *g; i++)
		((unsigned char *)g)[i] = 0;
	return 1;
}

/*
 * Open a single device. Honors MCC_GPU_DEVICE (0-based ordering index, or a
 * case-insensitive substring of the device name); otherwise takes the highest
 * scored. Returns 0 on success (g populated), nonzero on failure (CPU fallback).
 */
static int mcc_gpu_open(mcc_gpu *g) {
	VkPhysicalDevice list[16];
	const char *pin = getenv("MCC_GPU_DEVICE");
	int pin_idx = (pin && __mcc_gpu_is_num(pin)) ? atoi(pin) : -1;
	int n, pick = 0, k;
	unsigned i;
	for (i = 0; i < sizeof *g; i++)
		((unsigned char *)g)[i] = 0;
	if (__mcc_gpu_make_instance(&g->inst))
		return 1;
	n = __mcc_gpu_list(g->inst, list, 16);
	if (n <= 0)
		goto fail;
	if (pin_idx >= 0) {
		if (pin_idx >= n)
			goto fail;
		pick = pin_idx;
	} else if (pin) {
		pick = -1;
		for (k = 0; k < n; k++) {
			VkPhysicalDeviceProperties p;
			vkGetPhysicalDeviceProperties(list[k], &p);
			if (__mcc_gpu_ci_contains(p.deviceName, pin)) {
				pick = k;
				break;
			}
		}
		if (pick < 0)
			goto fail;
	}
	g->phys = list[pick];
	if (__mcc_gpu_finish(g))
		goto fail;
	return 0;
fail:
	if (g->dev)
		vkDestroyDevice(g->dev, 0);
	if (g->inst)
		vkDestroyInstance(g->inst, 0);
	for (i = 0; i < sizeof *g; i++)
		((unsigned char *)g)[i] = 0;
	return 1;
}

/*
 * Allocate the single host-visible/coherent SSBO (binding 0), persistently
 * mapped. Returns the mapping (memcpy target for upload/readback) or 0.
 */
static void *mcc_gpu_buffer(mcc_gpu *g, unsigned long nbytes) {
	VkBufferCreateInfo bi;
	VkMemoryRequirements mr;
	VkPhysicalDeviceMemoryProperties mp;
	VkMemoryAllocateInfo mai;
	unsigned j, mt = 0xffffffffu;
	for (j = 0; j < sizeof bi; j++) ((unsigned char *)&bi)[j] = 0;
	bi.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bi.size = nbytes;
	bi.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(g->dev, &bi, 0, &g->buf) != VK_SUCCESS)
		return 0;
	vkGetBufferMemoryRequirements(g->dev, g->buf, &mr);
	vkGetPhysicalDeviceMemoryProperties(g->phys, &mp);
	for (j = 0; j < mp.memoryTypeCount; j++) {
		unsigned need = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		if ((mr.memoryTypeBits & (1u << j)) &&
		    (mp.memoryTypes[j].propertyFlags & need) == need) {
			mt = j;
			break;
		}
	}
	if (mt == 0xffffffffu)
		return 0;
	for (j = 0; j < sizeof mai; j++) ((unsigned char *)&mai)[j] = 0;
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = mr.size;
	mai.memoryTypeIndex = mt;
	if (vkAllocateMemory(g->dev, &mai, 0, &g->mem) != VK_SUCCESS)
		return 0;
	if (vkBindBufferMemory(g->dev, g->buf, g->mem, 0) != VK_SUCCESS)
		return 0;
	if (vkMapMemory(g->dev, g->mem, 0, nbytes, 0, &g->map) != VK_SUCCESS)
		return 0;
	g->bufbytes = nbytes;
	return g->map;
}

/*
 * Build the compute pipeline from SPIR-V bytes (embedded array or file image),
 * with one storage-buffer binding and a push_sz-byte push-constant block.
 * Returns 0 on success.
 */
static int mcc_gpu_pipeline(mcc_gpu *g, const void *spv, unsigned long spv_bytes,
                            unsigned push_sz) {
	unsigned j;
	{
		VkShaderModuleCreateInfo si;
		for (j = 0; j < sizeof si; j++) ((unsigned char *)&si)[j] = 0;
		si.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		si.codeSize = spv_bytes;
		si.pCode = (const uint32_t *)spv;
		if (vkCreateShaderModule(g->dev, &si, 0, &g->shader) != VK_SUCCESS)
			return 1;
	}
	{
		VkDescriptorSetLayoutBinding b;
		VkDescriptorSetLayoutCreateInfo li;
		for (j = 0; j < sizeof b; j++) ((unsigned char *)&b)[j] = 0;
		for (j = 0; j < sizeof li; j++) ((unsigned char *)&li)[j] = 0;
		b.binding = 0;
		b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		b.descriptorCount = 1;
		b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		li.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		li.bindingCount = 1;
		li.pBindings = &b;
		if (vkCreateDescriptorSetLayout(g->dev, &li, 0, &g->dsl) != VK_SUCCESS)
			return 1;
	}
	{
		VkDescriptorPoolSize ps;
		VkDescriptorPoolCreateInfo pi;
		VkDescriptorSetAllocateInfo ai;
		VkDescriptorBufferInfo bufi;
		VkWriteDescriptorSet w;
		for (j = 0; j < sizeof ps; j++) ((unsigned char *)&ps)[j] = 0;
		for (j = 0; j < sizeof pi; j++) ((unsigned char *)&pi)[j] = 0;
		ps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps.descriptorCount = 1;
		pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pi.maxSets = 1;
		pi.poolSizeCount = 1;
		pi.pPoolSizes = &ps;
		if (vkCreateDescriptorPool(g->dev, &pi, 0, &g->dpool) != VK_SUCCESS)
			return 1;
		for (j = 0; j < sizeof ai; j++) ((unsigned char *)&ai)[j] = 0;
		ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		ai.descriptorPool = g->dpool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &g->dsl;
		if (vkAllocateDescriptorSets(g->dev, &ai, &g->dset) != VK_SUCCESS)
			return 1;
		for (j = 0; j < sizeof bufi; j++) ((unsigned char *)&bufi)[j] = 0;
		bufi.buffer = g->buf;
		bufi.offset = 0;
		bufi.range = g->bufbytes;
		for (j = 0; j < sizeof w; j++) ((unsigned char *)&w)[j] = 0;
		w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		w.dstSet = g->dset;
		w.dstBinding = 0;
		w.descriptorCount = 1;
		w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		w.pBufferInfo = &bufi;
		vkUpdateDescriptorSets(g->dev, 1, &w, 0, 0);
	}
	{
		VkPushConstantRange pcr;
		VkPipelineLayoutCreateInfo pli;
		VkComputePipelineCreateInfo cpi;
		for (j = 0; j < sizeof pcr; j++) ((unsigned char *)&pcr)[j] = 0;
		for (j = 0; j < sizeof pli; j++) ((unsigned char *)&pli)[j] = 0;
		pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		pcr.offset = 0;
		pcr.size = push_sz;
		pli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pli.setLayoutCount = 1;
		pli.pSetLayouts = &g->dsl;
		pli.pushConstantRangeCount = push_sz ? 1 : 0;
		pli.pPushConstantRanges = push_sz ? &pcr : 0;
		if (vkCreatePipelineLayout(g->dev, &pli, 0, &g->playout) != VK_SUCCESS)
			return 1;
		for (j = 0; j < sizeof cpi; j++) ((unsigned char *)&cpi)[j] = 0;
		cpi.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		cpi.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		cpi.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		cpi.stage.module = g->shader;
		cpi.stage.pName = "main";
		cpi.layout = g->playout;
		if (vkCreateComputePipelines(g->dev, VK_NULL_HANDLE, 1, &cpi, 0, &g->pipe) != VK_SUCCESS)
			return 1;
	}
	{
		VkCommandPoolCreateInfo pci;
		VkCommandBufferAllocateInfo cai;
		VkFenceCreateInfo fi;
		for (j = 0; j < sizeof pci; j++) ((unsigned char *)&pci)[j] = 0;
		pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		pci.queueFamilyIndex = g->qfam;
		if (vkCreateCommandPool(g->dev, &pci, 0, &g->cpool) != VK_SUCCESS)
			return 1;
		for (j = 0; j < sizeof cai; j++) ((unsigned char *)&cai)[j] = 0;
		cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cai.commandPool = g->cpool;
		cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cai.commandBufferCount = 1;
		if (vkAllocateCommandBuffers(g->dev, &cai, &g->cbuf) != VK_SUCCESS)
			return 1;
		for (j = 0; j < sizeof fi; j++) ((unsigned char *)&fi)[j] = 0;
		fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		if (vkCreateFence(g->dev, &fi, 0, &g->fence) != VK_SUCCESS)
			return 1;
	}
	return 0;
}

/*
 * Record + submit one dispatch of groups_x workgroups with the given push
 * constants, and block until it completes. Returns 0 on success. The mapped
 * buffer holds the results on return (host-coherent: no invalidate needed).
 */
static int mcc_gpu_run(mcc_gpu *g, const void *push, unsigned push_sz,
                       unsigned groups_x) {
	VkCommandBufferBeginInfo bi;
	VkSubmitInfo si;
	unsigned j;
	for (j = 0; j < sizeof bi; j++) ((unsigned char *)&bi)[j] = 0;
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(g->cbuf, &bi) != VK_SUCCESS)
		return 1;
	vkCmdBindPipeline(g->cbuf, VK_PIPELINE_BIND_POINT_COMPUTE, g->pipe);
	vkCmdBindDescriptorSets(g->cbuf, VK_PIPELINE_BIND_POINT_COMPUTE, g->playout,
	                        0, 1, &g->dset, 0, 0);
	if (push_sz)
		vkCmdPushConstants(g->cbuf, g->playout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
		                   push_sz, push);
	vkCmdDispatch(g->cbuf, groups_x, 1, 1);
	if (vkEndCommandBuffer(g->cbuf) != VK_SUCCESS)
		return 1;
	for (j = 0; j < sizeof si; j++) ((unsigned char *)&si)[j] = 0;
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &g->cbuf;
	vkResetFences(g->dev, 1, &g->fence);
	if (vkQueueSubmit(g->queue, 1, &si, g->fence) != VK_SUCCESS)
		return 1;
	if (vkWaitForFences(g->dev, 1, &g->fence, VK_TRUE, ~0ULL) != VK_SUCCESS)
		return 1;
	return 0;
}

static void mcc_gpu_close(mcc_gpu *g) {
	if (!g->dev)
		return;
	vkDeviceWaitIdle(g->dev);
	if (g->fence) vkDestroyFence(g->dev, g->fence, 0);
	if (g->cpool) vkDestroyCommandPool(g->dev, g->cpool, 0);
	if (g->pipe) vkDestroyPipeline(g->dev, g->pipe, 0);
	if (g->playout) vkDestroyPipelineLayout(g->dev, g->playout, 0);
	if (g->dpool) vkDestroyDescriptorPool(g->dev, g->dpool, 0);
	if (g->dsl) vkDestroyDescriptorSetLayout(g->dev, g->dsl, 0);
	if (g->shader) vkDestroyShaderModule(g->dev, g->shader, 0);
	if (g->map) vkUnmapMemory(g->dev, g->mem);
	if (g->mem) vkFreeMemory(g->dev, g->mem, 0);
	if (g->buf) vkDestroyBuffer(g->dev, g->buf, 0);
	vkDestroyDevice(g->dev, 0);
	if (g->inst) vkDestroyInstance(g->inst, 0);
}

#endif
