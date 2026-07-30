#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vulkan/vulkan.h>

#define WGSIZE 256
#define NGROUPS 4096
#define CHUNK (1u << 28)

#define VK(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { \
    printf("vulkan call failed line %d rc=%d\n", __LINE__, (int)_r); exit(1); } } while (0)

struct pc { unsigned int mode, fixed_half, base, count; };

static unsigned int LAT[160];
static int NLAT;

static void build_lattice(void) {
    int i, n = 0;
    LAT[n++] = 0u; LAT[n++] = 1u; LAT[n++] = 2u;
    LAT[n++] = 0xFFFFFFFFu; LAT[n++] = 0xFFFFFFFEu;
    LAT[n++] = 0x80000000u; LAT[n++] = 0x7FFFFFFFu;
    LAT[n++] = 0x55555555u; LAT[n++] = 0xAAAAAAAAu;
    LAT[n++] = 0x0F0F0F0Fu; LAT[n++] = 0xF0F0F0F0u;
    for (i = 0; i < 32; i++) LAT[n++] = 1u << i;
    for (i = 0; i < 32; i++) LAT[n++] = ~(1u << i);
    for (i = 0; i < 32; i++) LAT[n++] = (i == 0) ? 0u : ((1u << i) - 1u);
    for (i = 0; i < 32; i++) LAT[n++] = (unsigned int)(-(int)(1u << i));
    NLAT = n;
}

static unsigned int *load_spv(const char *p, size_t *nb) {
    FILE *f = fopen(p, "rb");
    unsigned int *b;
    long n;
    if (!f) { printf("cannot open %s\n", p); exit(1); }
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    b = malloc((size_t)n);
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { printf("short read\n"); exit(1); }
    fclose(f);
    *nb = (size_t)n;
    return b;
}

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

int main(int argc, char **argv) {
    unsigned long long total = (argc > 1) ? strtoull(argv[1], 0, 0) : (1ULL << 32);
    const char *spv = (argc > 2) ? argv[2] : "halves.spv";

    VkInstance inst;
    VkPhysicalDevice phys = 0;
    VkDevice dev;
    VkQueue q;
    VkBuffer buf;
    VkDeviceMemory mem;
    VkDescriptorSetLayout dsl;
    VkDescriptorPool dpool;
    VkDescriptorSet dset;
    VkPipelineLayout play;
    VkPipeline pipe;
    VkShaderModule sm;
    VkCommandPool cpool;
    VkCommandBuffer cb;
    VkFence fence;
    VkPhysicalDeviceProperties props;

    unsigned int qfam = 0xFFFFFFFFu;
    unsigned int ndev = 0, nq = 0, i;
    VkPhysicalDevice devs[8];
    VkQueueFamilyProperties qf[32];
    unsigned int *code;
    size_t nb;
    unsigned int *map;
    unsigned int res[5];
    double t0, t1;
    unsigned long long done = 0;
    unsigned long long swept_total = 0;

    VkApplicationInfo ai;
    VkInstanceCreateInfo ici;
    float prio = 1.0f;
    VkDeviceQueueCreateInfo qci;
    VkDeviceCreateInfo dci;
    VkBufferCreateInfo bci;
    VkMemoryRequirements mr;
    VkPhysicalDeviceMemoryProperties mp;
    VkMemoryAllocateInfo mai;
    VkDescriptorSetLayoutBinding dslb;
    VkDescriptorSetLayoutCreateInfo dslci;
    VkDescriptorPoolSize dps;
    VkDescriptorPoolCreateInfo dpci;
    VkDescriptorSetAllocateInfo dsai;
    VkDescriptorBufferInfo dbi;
    VkWriteDescriptorSet wds;
    VkShaderModuleCreateInfo smci;
    VkPushConstantRange pcr;
    VkPipelineLayoutCreateInfo plci;
    VkComputePipelineCreateInfo cpci;
    VkCommandPoolCreateInfo cpoolci;
    VkCommandBufferAllocateInfo cbai;
    VkFenceCreateInfo fci;

    memset(&ai, 0, sizeof ai);
    ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    ai.pApplicationName = "rev64vk";
    ai.apiVersion = VK_API_VERSION_1_1;
    memset(&ici, 0, sizeof ici);
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &ai;
    VK(vkCreateInstance(&ici, 0, &inst));

    ndev = 8;
    VK(vkEnumeratePhysicalDevices(inst, &ndev, devs));
    if (!ndev) { printf("no vulkan device\n"); return 1; }
    phys = devs[0];
    vkGetPhysicalDeviceProperties(phys, &props);

    nq = 32;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &nq, qf);
    for (i = 0; i < nq; i++)
        if (qf[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { qfam = i; break; }
    if (qfam == 0xFFFFFFFFu) { printf("no compute queue\n"); return 1; }

    memset(&qci, 0, sizeof qci);
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = qfam;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    memset(&dci, 0, sizeof dci);
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    VK(vkCreateDevice(phys, &dci, 0, &dev));
    vkGetDeviceQueue(dev, qfam, 0, &q);

    memset(&bci, 0, sizeof bci);
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = 32;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK(vkCreateBuffer(dev, &bci, 0, &buf));
    vkGetBufferMemoryRequirements(dev, buf, &mr);
    vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    memset(&mai, 0, sizeof mai);
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size;
    mai.memoryTypeIndex = 0xFFFFFFFFu;
    for (i = 0; i < mp.memoryTypeCount; i++) {
        unsigned int want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if ((mr.memoryTypeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & want) == want) {
            mai.memoryTypeIndex = i;
            break;
        }
    }
    if (mai.memoryTypeIndex == 0xFFFFFFFFu) { printf("no host-visible mem\n"); return 1; }
    VK(vkAllocateMemory(dev, &mai, 0, &mem));
    VK(vkBindBufferMemory(dev, buf, mem, 0));
    VK(vkMapMemory(dev, mem, 0, 32, 0, (void **)&map));
    memset(map, 0, 32);

    memset(&dslb, 0, sizeof dslb);
    dslb.binding = 0;
    dslb.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dslb.descriptorCount = 1;
    dslb.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    memset(&dslci, 0, sizeof dslci);
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 1;
    dslci.pBindings = &dslb;
    VK(vkCreateDescriptorSetLayout(dev, &dslci, 0, &dsl));

    dps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    dps.descriptorCount = 1;
    memset(&dpci, 0, sizeof dpci);
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &dps;
    VK(vkCreateDescriptorPool(dev, &dpci, 0, &dpool));
    memset(&dsai, 0, sizeof dsai);
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = dpool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &dsl;
    VK(vkAllocateDescriptorSets(dev, &dsai, &dset));
    dbi.buffer = buf; dbi.offset = 0; dbi.range = 32;
    memset(&wds, 0, sizeof wds);
    wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wds.dstSet = dset;
    wds.dstBinding = 0;
    wds.descriptorCount = 1;
    wds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    wds.pBufferInfo = &dbi;
    vkUpdateDescriptorSets(dev, 1, &wds, 0, 0);

    code = load_spv(spv, &nb);
    memset(&smci, 0, sizeof smci);
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = nb;
    smci.pCode = code;
    VK(vkCreateShaderModule(dev, &smci, 0, &sm));

    pcr.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(struct pc);
    memset(&plci, 0, sizeof plci);
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &dsl;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    VK(vkCreatePipelineLayout(dev, &plci, 0, &play));

    memset(&cpci, 0, sizeof cpci);
    cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    cpci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    cpci.stage.module = sm;
    cpci.stage.pName = "main";
    cpci.layout = play;
    VK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &cpci, 0, &pipe));

    memset(&cpoolci, 0, sizeof cpoolci);
    cpoolci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpoolci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    cpoolci.queueFamilyIndex = qfam;
    VK(vkCreateCommandPool(dev, &cpoolci, 0, &cpool));
    memset(&cbai, 0, sizeof cbai);
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = cpool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VK(vkAllocateCommandBuffers(dev, &cbai, &cb));
    memset(&fci, 0, sizeof fci);
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK(vkCreateFence(dev, &fci, 0, &fence));

    build_lattice();
    if (argc > 3) NLAT = atoi(argv[3]);
    printf("device: %s\n", props.deviceName);
    printf("lattice=%d values, 2 modes, %llu per sweep\n", NLAT, total);

    t0 = now();
    {
    int mode, li;
    for (mode = 0; mode < 2; mode++)
    for (li = 0; li < NLAT; li++) {
    done = 0;
    while (done < total) {
        unsigned long long left = total - done;
        struct pc p;
        VkCommandBufferBeginInfo bi;
        VkSubmitInfo si;

        p.mode = (unsigned int)mode;
        p.fixed_half = LAT[li];
        p.base = (unsigned int)done;
        p.count = (left > (unsigned long long)CHUNK) ? CHUNK : (unsigned int)left;

        memset(&bi, 0, sizeof bi);
        bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK(vkBeginCommandBuffer(cb, &bi));
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, pipe);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, play, 0, 1, &dset, 0, 0);
        vkCmdPushConstants(cb, play, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof p, &p);
        vkCmdDispatch(cb, NGROUPS, 1, 1);
        VK(vkEndCommandBuffer(cb));

        memset(&si, 0, sizeof si);
        si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        si.commandBufferCount = 1;
        si.pCommandBuffers = &cb;
        VK(vkResetFences(dev, 1, &fence));
        VK(vkQueueSubmit(q, 1, &si, fence));
        VK(vkWaitForFences(dev, 1, &fence, VK_TRUE, ~0ULL));

        done += p.count;
        swept_total += p.count;
    }
    }
    }
    t1 = now();

    res[0] = map[0]; res[1] = map[1]; res[2] = map[2]; res[3] = map[3]; res[4] = map[4];

    printf("swept=%llu wall=%.3fs rate=%.1f M/s\n",
           swept_total, t1 - t0, (double)swept_total / (t1 - t0) / 1e6);
    printf("bad_rev=%u bad_pop=%u bad_mul=%u\n", res[2], res[3], res[4]);

    vkDestroyFence(dev, fence, 0);
    vkDestroyCommandPool(dev, cpool, 0);
    vkDestroyPipeline(dev, pipe, 0);
    vkDestroyPipelineLayout(dev, play, 0);
    vkDestroyShaderModule(dev, sm, 0);
    vkDestroyDescriptorPool(dev, dpool, 0);
    vkDestroyDescriptorSetLayout(dev, dsl, 0);
    vkUnmapMemory(dev, mem);
    vkFreeMemory(dev, mem, 0);
    vkDestroyBuffer(dev, buf, 0);
    vkDestroyDevice(dev, 0);
    vkDestroyInstance(inst, 0);
    free(code);
    return (res[2] == 0 && res[3] == 0 && res[4] == 0) ? 0 : 1;
}
