#include "memory.h"
#include "helpers.h"
#include "types.h"

#include <vulkan/vulkan.h>

#include <print>

namespace svet::renderer {

MemoryPool createMemoryPool(LContext context,
                            const MemoryPoolSpecification &spec) {
  if (spec.bufferUsage == BufferUsage::NONE &&
      spec.imageUsage == ImageUsage::NONE) {
    std::println("[Vulkan] Memory pool must be created for specific resources "
                 "(buffers, images)");
    return nullptr;
  }

  VkMemoryPropertyFlags properties = getMemoryProperties(spec.properties);

  uint32_t memTypeBits = 0xFFFFFFFF;
  if (spec.bufferUsage != BufferUsage::NONE) {
    // Create dummy buffer with specified usage
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 1;
    bufferInfo.usage = getBufferUsage(spec.bufferUsage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer dummyBuffer;
    vkCreateBuffer(context->device, &bufferInfo, nullptr, &dummyBuffer);

    VkMemoryRequirements bufferReqs;
    vkGetBufferMemoryRequirements(context->device, dummyBuffer, &bufferReqs);

    // Create dummy image with specified usage (if imageUsage is set)
    memTypeBits &= bufferReqs.memoryTypeBits;

    vkDestroyBuffer(context->device, dummyBuffer, nullptr);
  }

  if (spec.imageUsage != ImageUsage::NONE) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    // TODO: This might need to be taken into account
    if ((spec.imageUsage & ImageUsage::RENDER_TARGET_DEPTH_STENCIL) ==
        ImageUsage::RENDER_TARGET_DEPTH_STENCIL) {
      imageInfo.format = VK_FORMAT_D32_SFLOAT;
    } else {
      imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    }
    imageInfo.extent = {1, 1, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = getImageTiling(spec.imageTiling);
    imageInfo.usage = getImageUsage(spec.imageUsage);
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage dummyImage;
    vkCreateImage(context->device, &imageInfo, nullptr, &dummyImage);

    VkMemoryRequirements imageReqs;
    vkGetImageMemoryRequirements(context->device, dummyImage, &imageReqs);

    // Intersect type bits (only use types compatible with BOTH)
    memTypeBits &= imageReqs.memoryTypeBits;

    vkDestroyImage(context->device, dummyImage, nullptr);
  }

  uint32_t memTypeIndexes =
      findMemoryTypes(context->physicalDevice, memTypeBits, properties);

  uint32_t memTypeIndex = 0;

  while (memTypeIndexes) {
    if ((memTypeIndexes & 1) == 0) {
      ++memTypeIndex;
      memTypeIndexes >>= 1;
      continue;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = spec.size;
    allocInfo.memoryTypeIndex = memTypeIndex;

    VkDeviceMemory memory;
    if (vkAllocateMemory(context->device, &allocInfo, nullptr, &memory) !=
        VK_SUCCESS) {
      ++memTypeIndex;
      memTypeIndexes >>= 1;
      continue;
    }

    auto memoryPool = new MemoryPoolT;
    memoryPool->memory = memory;
    memoryPool->size = spec.size;
    memoryPool->offset = 0;
    memoryPool->memoryTypeIndex = memTypeIndex;
    memoryPool->properties = properties;
    return memoryPool;
  }

  std::println("[Vulkan] No available memory fulfilling requirements");
  return nullptr;
}

void destroyMemoryPool(LContext context, MemoryPool memoryPool) {
  if (memoryPool->size / (1024 * 1024) == 0) {
    std::println("Pool used {:.1f}/{:.1f} KB - {:.2f}%",
                 memoryPool->offset / 1024.f, memoryPool->size / 1024.f,
                 memoryPool->offset / float(memoryPool->size) * 100.f);
  } else {
    std::println("Pool used {:.1f}/{:.1f} MB - {:.2f}%",
                 memoryPool->offset / (1024.f * 1024.f),
                 memoryPool->size / (1024.f * 1024.f),
                 memoryPool->offset / float(memoryPool->size) * 100.f);
  }
  vkFreeMemory(context->device, memoryPool->memory, nullptr);
  delete memoryPool;
}

} // namespace svet::renderer
