#pragma once
#include "context.h"
#include "enums.h"
#include "types.h"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <print>

namespace svet::renderer {

inline VkSurfaceFormatKHR chooseSwapchainFormat(VkPhysicalDevice physicalDevice,
                                                VkSurfaceKHR surface) {
  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       nullptr);

  VkSurfaceFormatKHR *formats =
      (VkSurfaceFormatKHR *)malloc(formatCount * sizeof(VkSurfaceFormatKHR));
  vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount,
                                       formats);

  VkSurfaceFormatKHR chosenFormat = formats[0];

  // Prefer sRGB format
  for (uint32_t i = 0; i < formatCount; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      chosenFormat = formats[i];
      break;
    }
  }

  free(formats);
  return chosenFormat;
}

inline VkPresentModeKHR
chooseSwapchainPresentMode(VkPhysicalDevice physicalDevice,
                           VkSurfaceKHR surface, bool vSync) {
  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &presentModeCount, nullptr);

  VkPresentModeKHR *presentModes = new VkPresentModeKHR[presentModeCount];
  vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                                            &presentModeCount, presentModes);

  // Always available (double-buffering, vsynced)
  VkPresentModeKHR chosenMode = VK_PRESENT_MODE_FIFO_KHR;

  for (uint32_t i = 0; i < presentModeCount; i++) {
    if (vSync) {
      if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
        // Triple buffering
        chosenMode = VK_PRESENT_MODE_MAILBOX_KHR;
        break;
      }
    } else {
      if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        chosenMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        break;
      }
    }
  }

  delete[] presentModes;
  return chosenMode;
}

inline VkExtent2D chooseSwapchainExtent(VkPhysicalDevice physicalDevice,
                                        VkSurfaceKHR surface, int windowWidth,
                                        int windowHeight) {
  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface,
                                            &capabilities);

  if (capabilities.currentExtent.width != UINT32_MAX) {
    return capabilities.currentExtent;
  }

  VkExtent2D extent = {.width = (uint32_t)windowWidth,
                       .height = (uint32_t)windowHeight};

  extent.width = (extent.width < capabilities.minImageExtent.width)
                     ? capabilities.minImageExtent.width
                 : (extent.width > capabilities.maxImageExtent.width)
                     ? capabilities.maxImageExtent.width
                     : extent.width;

  extent.height = (extent.height < capabilities.minImageExtent.height)
                      ? capabilities.minImageExtent.height
                  : (extent.height > capabilities.maxImageExtent.height)
                      ? capabilities.maxImageExtent.height
                      : extent.height;

  return extent;
}

inline uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                               uint32_t typeFilter,
                               VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      return i;
    }
  }

  // Fallback
  return 0;
}

inline uint32_t findMemoryTypes(VkPhysicalDevice physicalDevice,
                                uint32_t typeFilter,
                                VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

  uint32_t res = 0;
  for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags &
                                    properties) == properties) {
      res |= 1 << i;
    }
  }

  // Fallback
  return res;
}

inline VkBufferUsageFlags getBufferUsage(BufferUsage usage) {
  return (VkBufferUsageFlags)usage;
}

inline VkImageUsageFlags getImageUsage(ImageUsage imageUsage) {
  return (VkImageUsageFlags)imageUsage;
}

inline VkImageTiling getImageTiling(ImageTiling imageTiling) {
  switch (imageTiling) {
  case ImageTiling::OPTIMAL:
    return VK_IMAGE_TILING_OPTIMAL;
  case ImageTiling::LINEAR:
    return VK_IMAGE_TILING_LINEAR;
  }

  std::println("[Vulkan] Unknown image tiling, defaulting to OPTIMAL");
  return VK_IMAGE_TILING_OPTIMAL;
}

inline VkMemoryPropertyFlags getMemoryProperties(MemoryProperties properties) {
  return (VkMemoryPropertyFlags)properties;
}

struct AllocatedMemory {
  VkDeviceMemory memory;
  size_t offset;
};

inline AllocatedMemory allocateMemory(MemoryPool pool, size_t size,
                                      size_t alignment) {
  AllocatedMemory memory;
  memory.memory = pool->memory;
  memory.offset = (pool->offset + alignment - 1) & ~(alignment - 1);
  pool->offset = memory.offset + size;
  if (pool->offset > pool->size) {
    memory.memory = VK_NULL_HANDLE;
    memory.offset = 0;
  }
  return memory;
}

inline void freeMemory(MemoryPool pool, size_t offset) {
  //
}

inline uint32_t
determineOwningSrcQueueFamilyIndex(LContext context,
                                   QueueOwnershipState ownership) {
  if (ownership == QueueOwnershipState::GRAPHICS) {
    return context->graphicsQueueFamilyIndex;
  }
  if (ownership == QueueOwnershipState::TRANSFER) {
    return context->transferQueueFamilyIndex;
  }
  if (ownership == QueueOwnershipState::GRAPHICS_RELEASED_TO_TRANSFER) {
    return context->graphicsQueueFamilyIndex;
  }
  if (ownership == QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS) {
    return context->transferQueueFamilyIndex;
  }

  std::println("[Vulkan] Unsupported queue ownership, defaulting to NONE");
  return UINT32_MAX;
}

inline uint32_t
determineOwningDstQueueFamilyIndex(LContext context,
                                   QueueOwnershipState ownership) {
  if (ownership == QueueOwnershipState::GRAPHICS) {
    return context->graphicsQueueFamilyIndex;
  }
  if (ownership == QueueOwnershipState::TRANSFER) {
    return context->transferQueueFamilyIndex;
  }
  if (ownership == QueueOwnershipState::GRAPHICS_RELEASED_TO_TRANSFER) {
    return context->transferQueueFamilyIndex;
  }
  if (ownership == QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS) {
    return context->graphicsQueueFamilyIndex;
  }

  std::println("[Vulkan] Unsupported queue ownership, defaulting to NONE");
  return UINT32_MAX;
}

inline VkPipelineStageFlagBits2 getPipelineStages(PipelineStage stage) {
  return (VkPipelineStageFlagBits2)stage;
}

inline VkAccessFlagBits2 getResourceAccess(ResourceAccess access) {
  return (VkAccessFlagBits2)access;
}

inline void createBufferBarrier(LContext context, Buffer buffer,
                                const BufferMetadata &fromMeta,
                                const BufferMetadata &toMeta,
                                VkBufferMemoryBarrier2 &barrier) {
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
  barrier.pNext = nullptr;
  barrier.srcStageMask = getPipelineStages(fromMeta.stage);
  barrier.srcAccessMask = getResourceAccess(fromMeta.access);
  barrier.dstStageMask = getPipelineStages(toMeta.stage);
  barrier.dstAccessMask = getResourceAccess(toMeta.access);
  barrier.buffer = buffer->buffer;
  barrier.srcQueueFamilyIndex =
      determineOwningSrcQueueFamilyIndex(context, fromMeta.ownership);
  barrier.dstQueueFamilyIndex =
      determineOwningDstQueueFamilyIndex(context, toMeta.ownership);
  // TODO: Handle subresources properly
  barrier.offset = 0;
  barrier.size = buffer->size;
}

inline VkImageLayout getImageLayout(ImageLayout layout) {
  switch (layout) {
  case ImageLayout::UNDEFINED:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  case ImageLayout::GENERAL:
    return VK_IMAGE_LAYOUT_GENERAL;
  case ImageLayout::COLOR_RENDER_TARGET:
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  case ImageLayout::DEPTH_RENDER_TARGET:
    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
  case ImageLayout::SHADER_READ_ONLY:
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  case ImageLayout::TRANSFER_SRC:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case ImageLayout::TRANSFER_DST:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case ImageLayout::PRESENT:
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  }

  std::println("[Vulkan] Unsupported image layout, defaulting to UNDEFINED");
  return VK_IMAGE_LAYOUT_UNDEFINED;
}

inline void createImageBarrier(LContext context, Image image,
                               const ImageMetadata &fromMeta,
                               const ImageMetadata &toMeta,
                               VkImageMemoryBarrier2 &barrier) {
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.pNext = nullptr;
  barrier.oldLayout = getImageLayout(fromMeta.layout);
  barrier.newLayout = getImageLayout(toMeta.layout);
  barrier.srcStageMask = getPipelineStages(fromMeta.stage);
  barrier.srcAccessMask = getResourceAccess(fromMeta.access);
  barrier.dstStageMask = getPipelineStages(toMeta.stage);
  barrier.dstAccessMask = getResourceAccess(toMeta.access);
  barrier.srcQueueFamilyIndex =
      determineOwningSrcQueueFamilyIndex(context, fromMeta.ownership);
  barrier.dstQueueFamilyIndex =
      determineOwningDstQueueFamilyIndex(context, toMeta.ownership);
  barrier.image = image->image;
  if (image->isDepth) {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else {
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  // TODO: Handle subresources properly
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
}

inline VkCommandBuffer allocAndBeginTmpCmdBuffer(LContext context,
                                                 VkCommandPool commandPool) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandPool = commandPool;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer commandBuffer;
  vkAllocateCommandBuffers(context->device, &allocInfo, &commandBuffer);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  return commandBuffer;
}

inline void getResourcePairPoolAndQueue(LContext context,
                                        QueueOwnershipState first,
                                        QueueOwnershipState second,
                                        VkCommandPool &commandPool,
                                        VkQueue &queue,
                                        QueueOwnershipState &ownership) {
  if ((int)first > (int)second) {
    std::swap(first, second);
  }
  if (first == QueueOwnershipState::GRAPHICS &&
      second == QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS) {
    commandPool = context->graphicsCommandPool;
    queue = context->graphicsQueue;
    ownership = QueueOwnershipState::GRAPHICS;
  } else if (first == QueueOwnershipState::TRANSFER &&
             second == QueueOwnershipState::GRAPHICS_RELEASED_TO_TRANSFER) {
    commandPool = context->transferCommandPool;
    queue = context->transferQueue;
    ownership = QueueOwnershipState::TRANSFER;
  } else if (first == QueueOwnershipState::TRANSFER &&
             second == QueueOwnershipState::TRANSFER) {
    commandPool = context->transferCommandPool;
    queue = context->transferQueue;
    ownership = QueueOwnershipState::TRANSFER;
  } else if (first == QueueOwnershipState::GRAPHICS &&
             second == QueueOwnershipState::GRAPHICS) {
    commandPool = context->graphicsCommandPool;
    queue = context->graphicsQueue;
    ownership = QueueOwnershipState::GRAPHICS;
  } else {
    std::println("[Vulkan] Unsupported resource pair queue ownership, {} + {}",
                 (int)first, (int)second);
    return;
  }
}

inline void getTransitionPoolAndQueue(LContext context, QueueOwnershipState in,
                                      QueueOwnershipState out,
                                      VkCommandPool &commandPool,
                                      VkQueue &queue) {
  if (in == QueueOwnershipState::GRAPHICS &&
      out == QueueOwnershipState::GRAPHICS_RELEASED_TO_TRANSFER) {
    commandPool = context->graphicsCommandPool;
    queue = context->graphicsQueue;
  } else if (in == QueueOwnershipState::TRANSFER &&
             out == QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS) {
    commandPool = context->transferCommandPool;
    queue = context->transferQueue;
  } else if (in == QueueOwnershipState::GRAPHICS_RELEASED_TO_TRANSFER &&
             out == QueueOwnershipState::TRANSFER) {
    commandPool = context->transferCommandPool;
    queue = context->transferQueue;
  } else if (in == QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS &&
             out == QueueOwnershipState::GRAPHICS) {
    commandPool = context->graphicsCommandPool;
    queue = context->graphicsQueue;
  } else if (in == QueueOwnershipState::GRAPHICS &&
             out == QueueOwnershipState::GRAPHICS) {
    commandPool = context->graphicsCommandPool;
    queue = context->graphicsQueue;
  } else if (in == QueueOwnershipState::TRANSFER &&
             out == QueueOwnershipState::TRANSFER) {
    commandPool = context->transferCommandPool;
    queue = context->transferQueue;
  } else {
    std::println("[Vulkan] Unsupported queue ownership transition, {} -> {}",
                 (int)in, (int)out);
    return;
  }
}

inline void blitImage(VkCommandBuffer commandBuffer, VkImage srcImage,
                      VkImage dstImage, uint32_t srcWidth, uint32_t srcHeight,
                      uint32_t dstWidth, uint32_t dstHeight, VkFilter filter) {
  VkImageBlit blit{};
  blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit.srcSubresource.layerCount = 1;
  blit.srcOffsets[1] = {(int32_t)srcWidth, (int32_t)srcHeight, 1};

  blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  blit.dstSubresource.layerCount = 1;
  blit.dstOffsets[1] = {(int32_t)dstWidth, (int32_t)dstHeight, 1};

  vkCmdBlitImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                 filter);
}

inline void copyImage(VkCommandBuffer commandBuffer, VkImage srcImage,
                      VkImage dstImage, uint32_t width, uint32_t height) {
  VkImageCopy copy{};
  copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.srcSubresource.layerCount = 1;
  copy.srcOffset = {0, 0, 0};

  copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.dstSubresource.layerCount = 1;
  copy.dstOffset = {0, 0, 0};

  copy.extent = {width, height, 1};

  vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

inline void copyImage(VkCommandBuffer commandBuffer, VkImage srcImage,
                      VkImage dstImage, uint32_t srcX, uint32_t srcY,
                      uint32_t dstX, uint32_t dstY, uint32_t width,
                      uint32_t height) {
  VkImageCopy copy{};
  copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.srcSubresource.layerCount = 1;
  copy.srcOffset = {(int32_t)srcX, (int32_t)srcY, 0};

  copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.dstSubresource.layerCount = 1;
  copy.dstOffset = {(int32_t)dstX, (int32_t)dstY, 0};

  copy.extent = {width, height, 1};

  vkCmdCopyImage(commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
}

inline VkFilter getVulkanFilter(SampleFilter filter) {
  switch (filter) {
  case SampleFilter::NEAREST:
    return VK_FILTER_NEAREST;
  case SampleFilter::LINEAR:
    return VK_FILTER_LINEAR;
  }

  std::println("[Vulkan] Unsupported filter, defaulting to NEAREST");
  return VK_FILTER_NEAREST;
}

inline VkSamplerAddressMode getAddressMode(SampleAddressing addressing) {
  switch (addressing) {
  case SampleAddressing::REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case SampleAddressing::MIRRORED_REPEAT:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  case SampleAddressing::CLAMP_TO_EDGE:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case SampleAddressing::CLAMP_TO_BORDER:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
  case SampleAddressing::MIRROR_CLAMP_TO_EDGE:
    return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
  }

  std::println("[Vulkan] Unsupported addressing, defaulting to REPEAT");
  return VK_SAMPLER_ADDRESS_MODE_REPEAT;
}

inline VkDescriptorType getDescriptorType(ResourceType type) {
  switch (type) {
  case ResourceType::UNIFORM_BUFFER:
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  case ResourceType::SAMPLER:
    return VK_DESCRIPTOR_TYPE_SAMPLER;
  case ResourceType::SAMPLED_IMAGE:
    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  case ResourceType::STORAGE_IMAGE:
    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  case ResourceType::COMBINED_SAMPLER:
    return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  case ResourceType::STORAGE_BUFFER:
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  case ResourceType::STORAGE_TEXEL_BUFFER:
    return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
  }

  std::println("[Vulkan] Unknown uniform type, defaulting to UNIFORM_BUFFER");
  return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

inline VkFrontFace getFrontFace(FrontFaceWind wind) {
  if (wind == FrontFaceWind::CLOCKWISE)
    return VK_FRONT_FACE_CLOCKWISE;
  return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

inline VkShaderStageFlags getShaderStageFlags(ShaderVisibility visibility) {
  return (VkShaderStageFlags)visibility;
}

inline VkFormat getVkVertexFormat(VertexFormat format) {
  return (VkFormat)format;
  /*
  switch (format) {
  case VertexFormat::FLOAT:
    return VK_FORMAT_R32_SFLOAT;
  case VertexFormat::FLOAT2:
    return VK_FORMAT_R32G32_SFLOAT;
  case VertexFormat::FLOAT3:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case VertexFormat::FLOAT4:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case VertexFormat::INT:
    return VK_FORMAT_R32_SINT;
  case VertexFormat::INT2:
    return VK_FORMAT_R32G32_SINT;
  case VertexFormat::INT3:
    return VK_FORMAT_R32G32B32_SINT;
  case VertexFormat::INT4:
    return VK_FORMAT_R32G32B32A32_SINT;
  case VertexFormat::UINT:
    return VK_FORMAT_R32_UINT;
  case VertexFormat::UINT2:
    return VK_FORMAT_R32G32_UINT;
  case VertexFormat::UINT3:
    return VK_FORMAT_R32G32B32_UINT;
  case VertexFormat::UINT4:
    return VK_FORMAT_R32G32B32A32_UINT;
  default:
    return VK_FORMAT_UNDEFINED;
  }
    */
}

inline VkFormat getVulkanFormat(PixelFormat pixelFormat) {
  return (VkFormat)pixelFormat;
  /*
  switch (pixelFormat) {
  case PixelFormat::UINT8_RGBA_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case PixelFormat::UINT8_BGRA_SRGB:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case PixelFormat::UINT8_RGBA:
    return VK_FORMAT_R8G8B8A8_UINT;
  case PixelFormat::UINT8_BGRA:
    return VK_FORMAT_B8G8R8A8_UINT;
  case PixelFormat::UINT8_RGB_SRGB:
    return VK_FORMAT_R8G8B8_SRGB;
  case PixelFormat::UINT8_BGR_SRGB:
    return VK_FORMAT_B8G8R8_SRGB;
  case PixelFormat::UINT8_RGB:
    return VK_FORMAT_R8G8B8_UINT;
  case PixelFormat::UINT8_BGR:
    return VK_FORMAT_B8G8R8_UINT;
  case PixelFormat::UNORM8_RGBA:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case PixelFormat::UNORM8_BGRA:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case PixelFormat::UNORM8_RGB:
    return VK_FORMAT_R8G8B8_UNORM;
  case PixelFormat::UNORM8_BGR:
    return VK_FORMAT_B8G8R8_UNORM;
  case PixelFormat::UINT8_R_SRGB:
    return VK_FORMAT_R8_SRGB;
  case PixelFormat::UINT8_R:
    return VK_FORMAT_R8_UINT;
  case PixelFormat::UNORM8_R:
    return VK_FORMAT_R8_UNORM;
  case PixelFormat::UINT16_RGBA:
    return VK_FORMAT_R16G16B16A16_UINT;
  case PixelFormat::UINT16_RGB:
    return VK_FORMAT_R16G16B16_UINT;
  case PixelFormat::UNORM16_RGBA:
    return VK_FORMAT_R16G16B16A16_UNORM;
  case PixelFormat::UNORM16_RGB:
    return VK_FORMAT_R16G16B16_UNORM;
  case PixelFormat::UINT16_R:
    return VK_FORMAT_R16_UINT;
  case PixelFormat::UNORM16_R:
    return VK_FORMAT_R16_UNORM;
  case PixelFormat::UINT32_RGBA:
    return VK_FORMAT_R32G32B32A32_UINT;
  case PixelFormat::UINT32_RGB:
    return VK_FORMAT_R32G32B32_UINT;
  case PixelFormat::UINT32_R:
    return VK_FORMAT_R32_UINT;
  case PixelFormat::FLOAT32_RGBA:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case PixelFormat::FLOAT32_RGB:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case PixelFormat::FLOAT32_R:
    return VK_FORMAT_R32_SFLOAT;
  case PixelFormat::FLOAT16_RGBA:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case PixelFormat::FLOAT16_RGB:
    return VK_FORMAT_R16G16B16_SFLOAT;
  case PixelFormat::FLOAT16_R:
    return VK_FORMAT_R16_SFLOAT;
  case PixelFormat::UNORM16_D:
    return VK_FORMAT_D16_UNORM;
  case PixelFormat::FLOAT32_D:
    return VK_FORMAT_D32_SFLOAT;
  case PixelFormat::BC1_RGB:
    return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
  case PixelFormat::BC1_RGBA:
    return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
  case PixelFormat::BC1_RGB_SRGB:
    return VK_FORMAT_BC1_RGB_SRGB_BLOCK;
  case PixelFormat::BC1_RGBA_SRGB:
    return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
  case PixelFormat::BC2:
    return VK_FORMAT_BC2_UNORM_BLOCK;
  case PixelFormat::BC2_SRGB:
    return VK_FORMAT_BC2_SRGB_BLOCK;
  case PixelFormat::BC3:
    return VK_FORMAT_BC3_UNORM_BLOCK;
  case PixelFormat::BC3_SRGB:
    return VK_FORMAT_BC3_SRGB_BLOCK;
  case PixelFormat::BC4:
    return VK_FORMAT_BC4_UNORM_BLOCK;
  case PixelFormat::BC4_S:
    return VK_FORMAT_BC4_SNORM_BLOCK;
  case PixelFormat::BC5:
    return VK_FORMAT_BC5_UNORM_BLOCK;
  case PixelFormat::BC5_S:
    return VK_FORMAT_BC5_SNORM_BLOCK;
  case PixelFormat::BC6:
    return VK_FORMAT_BC6H_UFLOAT_BLOCK;
  case PixelFormat::BC6_S:
    return VK_FORMAT_BC6H_SFLOAT_BLOCK;
  case PixelFormat::BC7:
    return VK_FORMAT_BC7_UNORM_BLOCK;
  case PixelFormat::BC7_SRGB:
    return VK_FORMAT_BC7_SRGB_BLOCK;
  }

  std::println("[Vulkan] Unknown pixel format, defaulting to UINT8_RGBA_SRGB");
  return VK_FORMAT_R8G8B8A8_SRGB;
  */
}

} // namespace svet::renderer
