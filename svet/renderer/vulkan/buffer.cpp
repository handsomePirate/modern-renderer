#include "buffer.h"
#include "helpers.h"
#include "types.h"

#include <vulkan/vulkan.h>

#include <cstring>

namespace svet::renderer {

Buffer createBuffer(LContext context, const BufferSpecification &spec) {
  if (not spec.memoryPool) {
    std::println("[Vulkan] Memory pool required to allocate buffer");
    return nullptr;
  }
  Buffer buffer = new BufferT;

  if (spec.outMeta) {
    spec.outMeta->stage = PipelineStage::NONE;
    spec.outMeta->access = ResourceAccess::NONE;
    spec.outMeta->ownership = (QueueOwnershipState)spec.initialOwnership;
  }
  buffer->size = spec.size;

  VkBufferCreateInfo bufferInfo{};
  bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferInfo.size = spec.size;
  bufferInfo.usage = getBufferUsage(spec.usage);
  bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (vkCreateBuffer(context->device, &bufferInfo, nullptr, &buffer->buffer) !=
      VK_SUCCESS) {
    delete buffer;
    return nullptr;
  }

  VkMemoryRequirements memRequirements;
  vkGetBufferMemoryRequirements(context->device, buffer->buffer,
                                &memRequirements);

  uint32_t memTypeIndex =
      findMemoryType(context->physicalDevice, memRequirements.memoryTypeBits,
                     spec.memoryPool->properties);

  if (((1 << spec.memoryPool->memoryTypeIndex) &
       memRequirements.memoryTypeBits) == 0) {
    std::println("[Vulkan] Buffer incompatible with provided memory pool");
    vkDestroyBuffer(context->device, buffer->buffer, nullptr);
    delete buffer;
    return nullptr;
  }
  AllocatedMemory memory = allocateMemory(spec.memoryPool, memRequirements.size,
                                          memRequirements.alignment);

  buffer->memory = memory.memory;
  buffer->offset = memory.offset;
  buffer->pool = spec.memoryPool;

  if (buffer->memory == VK_NULL_HANDLE) {
    std::println(
        "[Vulkan] Failed to allocate buffer, memory pool ran out of memory");
    vkDestroyBuffer(context->device, buffer->buffer, nullptr);
    delete buffer;
    return nullptr;
  }

  vkBindBufferMemory(context->device, buffer->buffer, buffer->memory,
                     buffer->offset);

  return buffer;
}

void uploadBufferData(LContext context, Buffer buffer, const void *data,
                      size_t offset, size_t size) {
  void *mappedMemory = nullptr;
  vkMapMemory(context->device, buffer->memory, buffer->offset + offset, size, 0,
              &mappedMemory);
  std::memcpy(mappedMemory, data, size);
  vkUnmapMemory(context->device, buffer->memory);
}

void *mapBuffer(LContext context, Buffer buffer, size_t offset, size_t size) {
  void *mappedMemory = nullptr;
  vkMapMemory(context->device, buffer->memory, buffer->offset + offset, size, 0,
              &mappedMemory);
  return mappedMemory;
}

void flushBuffer(LContext context, Buffer buffer, size_t offset, size_t size) {
  VkMappedMemoryRange range{};
  range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
  range.memory = buffer->memory;
  range.offset = buffer->offset + offset;
  range.size = size;
  vkFlushMappedMemoryRanges(context->device, 1, &range);
}

void unmapBuffer(LContext context, Buffer buffer) {
  vkUnmapMemory(context->device, buffer->memory);
}

namespace {

inline void bufferCopyPlaceInitialBarrier(LContext context,
                                          const BufferCopySpecification &spec,
                                          QueueOwnershipState ownership,
                                          VkCommandBuffer commandBuffer) {
  VkBufferMemoryBarrier2 barriers[2];
  uint32_t barrierCount = 0;
  BufferMetadata sourceMeta{};
  sourceMeta.stage = PipelineStage::TRANSFER;
  sourceMeta.access = ResourceAccess::TRANSFER_READ;
  sourceMeta.ownership = ownership;
  if (sourceMeta.stage != spec.sourceMeta.stage ||
      sourceMeta.access != spec.sourceMeta.access ||
      sourceMeta.ownership != spec.sourceMeta.ownership) {
    createBufferBarrier(context, spec.source, spec.sourceMeta, sourceMeta,
                        barriers[barrierCount++]);
  }
  BufferMetadata destinationMeta{};
  destinationMeta.stage = PipelineStage::TRANSFER;
  destinationMeta.access = ResourceAccess::TRANSFER_WRITE;
  destinationMeta.ownership = ownership;
  createBufferBarrier(context, spec.destination, spec.destinationMeta,
                      destinationMeta, barriers[barrierCount++]);
  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pBufferMemoryBarriers = barriers;
  dependencyInfo.bufferMemoryBarrierCount = barrierCount;
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

inline void bufferCopyPlaceFinalBarrier(LContext context,
                                        const BufferCopySpecification &spec,
                                        QueueOwnershipState ownership,
                                        VkCommandBuffer commandBuffer) {
  BufferMetadata sourceMeta{};
  sourceMeta.stage = PipelineStage::TRANSFER;
  sourceMeta.access = ResourceAccess::TRANSFER_READ;
  sourceMeta.ownership = ownership;

  BufferMetadata destinationMeta{};
  destinationMeta.stage = PipelineStage::TRANSFER;
  destinationMeta.access = ResourceAccess::TRANSFER_WRITE;
  destinationMeta.ownership = ownership;

  VkBufferMemoryBarrier2 barriers[2];
  uint32_t barrierCount = 0;
  if ((QueueOwnershipState)spec.desiredSrcOwnership != ownership) {
    BufferMetadata finalSourceMeta{};
    finalSourceMeta.access = ResourceAccess::NONE;
    if (ownership == QueueOwnershipState::GRAPHICS) {
      finalSourceMeta.stage = PipelineStage::TRANSFER;
      finalSourceMeta.ownership =
          QueueOwnershipState::GRAPHICS_RELEASED_TO_TRANSFER;
    } else {
      finalSourceMeta.stage = PipelineStage::BOTTOM_OF_PIPE;
      finalSourceMeta.ownership =
          QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS;
    }
    createBufferBarrier(context, spec.source, sourceMeta, finalSourceMeta,
                        barriers[barrierCount++]);
    sourceMeta = finalSourceMeta;
  }

  if ((QueueOwnershipState)spec.desiredDstOwnership != ownership) {
    BufferMetadata finalDestinationMeta{};
    finalDestinationMeta.access = ResourceAccess::NONE;
    if (ownership == QueueOwnershipState::GRAPHICS) {
      finalDestinationMeta.stage = PipelineStage::TRANSFER;
      finalDestinationMeta.ownership =
          QueueOwnershipState::GRAPHICS_RELEASED_TO_TRANSFER;
    } else {
      finalDestinationMeta.stage = PipelineStage::BOTTOM_OF_PIPE;
      finalDestinationMeta.ownership =
          QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS;
    }

    createBufferBarrier(context, spec.destination, destinationMeta,
                        finalDestinationMeta, barriers[barrierCount++]);
    destinationMeta = finalDestinationMeta;
  }

  if (barrierCount > 0) {
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pBufferMemoryBarriers = barriers;
    dependencyInfo.bufferMemoryBarrierCount = barrierCount;
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
  }

  if (spec.outSourceMeta) {
    *spec.outSourceMeta = sourceMeta;
  }
  if (spec.outDestinationMeta) {
    *spec.outDestinationMeta = destinationMeta;
  }
}

} // namespace

void copyBufferData(LContext context, const BufferCopySpecification &spec) {

  VkCommandPool commandPool;
  VkQueue queue;
  QueueOwnershipState ownership;
  getResourcePairPoolAndQueue(context, spec.sourceMeta.ownership,
                              spec.destinationMeta.ownership, commandPool,
                              queue, ownership);
  if (commandPool == VK_NULL_HANDLE) {
    return;
  }

  VkCommandBuffer commandBuffer =
      allocAndBeginTmpCmdBuffer(context, commandPool);

  bufferCopyPlaceInitialBarrier(context, spec, ownership, commandBuffer);

  VkBufferCopy region{};
  region.srcOffset = spec.source->offset + spec.sourceOffset;
  region.dstOffset = spec.destination->offset + spec.destinationOffset;
  region.size = spec.size;

  vkCmdCopyBuffer(commandBuffer, spec.source->buffer, spec.destination->buffer,
                  1, &region);

  bufferCopyPlaceFinalBarrier(context, spec, ownership, commandBuffer);

  vkEndCommandBuffer(commandBuffer);

  // Submit and wait
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  vkFreeCommandBuffers(context->device, commandPool, 1, &commandBuffer);
}

void destroyBuffer(LContext context, Buffer buffer) {
  vkDestroyBuffer(context->device, buffer->buffer, nullptr);
  freeMemory(buffer->pool, buffer->offset);
  delete buffer;
}

void transitionBuffer(LContext context, Buffer buffer, BufferMetadata meta,
                      BufferMetadata outMeta) {
  VkCommandPool commandPool;
  VkQueue queue;
  getTransitionPoolAndQueue(context, meta.ownership, outMeta.ownership,
                            commandPool, queue);
  if (commandPool == VK_NULL_HANDLE) {
    return;
  }

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

  VkBufferMemoryBarrier2 bufferBarrier{};
  bufferBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  createBufferBarrier(context, buffer, meta, outMeta, bufferBarrier);

  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pBufferMemoryBarriers = &bufferBarrier;
  dependencyInfo.bufferMemoryBarrierCount = 1;
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

  vkEndCommandBuffer(commandBuffer);

  // Submit command buffer
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  // Cleanup
  vkFreeCommandBuffers(context->device, commandPool, 1, &commandBuffer);
}

} // namespace svet::renderer
