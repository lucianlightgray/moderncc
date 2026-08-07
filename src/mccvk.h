#ifndef MCC_VK_H
#define MCC_VK_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "mcchost.h"

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

#endif
