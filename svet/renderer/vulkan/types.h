#pragma once
#include "descriptor.h"
#include "enums.h"
#include "image.h"

#include <vulkan/vulkan.h>

struct SDL_Window;

namespace svet::renderer {

struct LContextT {
  SDL_Window *window;
  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;
  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice;
  VkDevice device;
  uint32_t graphicsQueueFamilyIndex;
  uint32_t transferQueueFamilyIndex;
  VkQueue graphicsQueue;
  VkQueue transferQueue;
  VkCommandPool graphicsCommandPool;
  VkCommandPool transferCommandPool;
  VkDeviceSize atomicFlushSize;
  bool allowValidation;
};

struct MemoryPoolT {
  VkDeviceMemory memory;
  VkDeviceSize size;
  size_t offset;
  uint32_t memoryTypeIndex;
  VkMemoryPropertyFlags properties;
};

struct BufferT {
  VkBuffer buffer;
  VkDeviceMemory memory;
  size_t offset;
  size_t size;
  MemoryPool pool;
};

struct ShaderT {
  VkShaderModule module;
};

struct PipelineLayoutT {
  VkPipelineLayout layout;
};

struct PipelineT {
  VkPipeline pipeline;
  VkPipelineLayout layout;
  bool isGraphics;
};

struct DescriptorSetLayoutT {
  VkDescriptorSetLayout descriptorSetLayout;
  ResourceType *types;
  uint32_t typeCount;
};

struct RenderPassT {
  Image *renderTargets;
  uint32_t renderTargetCount;
  Image depthTarget;
  VkRenderingAttachmentInfo *colorAttachmentInfos;
  VkRenderingAttachmentInfo depthAttachmentInfo;
  uint32_t width;
  uint32_t height;
};

struct ImageT {
  VkImage image;
  VkImageView imageView;
  VkDeviceMemory memory;
  size_t offset;
  VkFormat format;
  uint32_t width;
  uint32_t height;
  MemoryPool pool;
  bool isDepth;
};

struct SamplerT {
  VkSampler sampler;
};

struct SwapchainT {
  VkSwapchainKHR swapchain;
  uint32_t width;
  uint32_t height;
  VkFormat format;
  VkImage *images;
  VkImageView *imageViews;
  uint32_t imageCount;
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffer;
  VkQueue presentQueue;
  VkSemaphore blitSemaphore;
  VkSemaphore acquireSemaphore;
  uint64_t lastFrameTime;
  uint32_t stabilizeFramerate;
};

struct DescriptorPoolT {
  VkDescriptorPool descriptorPool;
  // TODO: Should have a chain of descriptor pools for unlimited capacity
};

struct DescriptorSetT {
  DescriptorPool pool;
  VkDescriptorSet descriptorSet;
};

struct DrawProcessorT {
  VkCommandPool commandPool;
  VkCommandBuffer primaryCommandBuffer;
  // TODO: Test and fix this
  VkCommandBuffer *secondaryCommandBuffers;
  bool *isRecorded;
  uint32_t secondaryCommandBufferCount;
  PipelineT *boundPipeline;
};

} // namespace svet::renderer
