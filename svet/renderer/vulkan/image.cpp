#include "image.h"
#include "helpers.h"
#include "types.h"

#include <vulkan/vulkan.h>

#include <print>

namespace svet::renderer {

namespace {

bool isDepthFormat(PixelFormat pixelFormat) {
  return (pixelFormat == PixelFormat::UNORM16_D ||
          pixelFormat == PixelFormat::FLOAT32_D);
}

} // namespace

Image createImage(LContext context, const ImageSpecification &spec) {
  auto image = new ImageT;
  image->width = spec.width;
  image->height = spec.height;
  image->format = getVulkanFormat(spec.pixelFormat);
  if (spec.outMeta) {
    spec.outMeta->layout = spec.initialLayout;
    spec.outMeta->ownership = (QueueOwnershipState)spec.initialOwnership;
    spec.outMeta->stage = PipelineStage::NONE;
    spec.outMeta->access = ResourceAccess::NONE;
  }

  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width = spec.width;
  imageInfo.extent.height = spec.height;
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = image->format;
  imageInfo.tiling = getImageTiling(spec.tiling);
  imageInfo.initialLayout = getImageLayout(spec.initialLayout);
  imageInfo.usage = getImageUsage(spec.usage);
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  auto result =
      vkCreateImage(context->device, &imageInfo, nullptr, &image->image);
  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to create render target image: {}",
                 (int)result);
    delete image;
    return nullptr;
  }

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(context->device, image->image, &memRequirements);

  if (((1 << spec.memoryPool->memoryTypeIndex) &
       memRequirements.memoryTypeBits) == 0) {
    std::println("[Vulkan] Image incompatible with provided memory pool");
    vkDestroyImage(context->device, image->image, nullptr);
    delete image;
    return nullptr;
  }
  AllocatedMemory memory = allocateMemory(spec.memoryPool, memRequirements.size,
                                          memRequirements.alignment);

  image->memory = memory.memory;
  image->offset = memory.offset;
  image->pool = spec.memoryPool;

  if (image->memory == VK_NULL_HANDLE) {
    std::println("[Vulkan] Failed to allocate render target memory");
    vkDestroyImage(context->device, image->image, nullptr);
    delete image;
    return nullptr;
  }

  vkBindImageMemory(context->device, image->image, image->memory,
                    image->offset);

  // Create VkImageView
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image->image;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = image->format;
  if (isDepthFormat(spec.pixelFormat)) {
    image->isDepth = true;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
  } else {
    image->isDepth = false;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = 1;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = 1;

  result =
      vkCreateImageView(context->device, &viewInfo, nullptr, &image->imageView);
  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to create render target image view: {}",
                 (int)result);
    freeMemory(image->pool, image->offset);
    vkDestroyImage(context->device, image->image, nullptr);
    delete image;
    return nullptr;
  }

  return image;
}

void destroyImage(LContext context, Image image) {
  vkDestroyImageView(context->device, image->imageView, nullptr);
  vkDestroyImage(context->device, image->image, nullptr);
  freeMemory(image->pool, image->offset);
  delete image;
}

namespace {

inline void imageDataCopyPlaceInitialBarrier(
    LContext context, const ImageDataCopySpecification &spec,
    QueueOwnershipState ownership, VkCommandBuffer commandBuffer) {
  VkBufferMemoryBarrier2 bufferBarrier{};
  VkImageMemoryBarrier2 imageBarrier{};

  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pBufferMemoryBarriers = nullptr;
  dependencyInfo.bufferMemoryBarrierCount = 0;

  BufferMetadata stagingMeta{};
  stagingMeta.stage = PipelineStage::TRANSFER;
  stagingMeta.access = ResourceAccess::TRANSFER_READ;
  stagingMeta.ownership = ownership;
  createBufferBarrier(context, spec.stagingBuffer, spec.stagingBufferMeta,
                      stagingMeta, bufferBarrier);
  if (stagingMeta.stage != spec.stagingBufferMeta.stage ||
      stagingMeta.access != spec.stagingBufferMeta.access ||
      stagingMeta.ownership != spec.stagingBufferMeta.ownership) {
    createBufferBarrier(context, spec.stagingBuffer, spec.stagingBufferMeta,
                        stagingMeta, bufferBarrier);
    dependencyInfo.pBufferMemoryBarriers = &bufferBarrier;
    dependencyInfo.bufferMemoryBarrierCount = 1;
  }
  ImageMetadata imageMeta = spec.imageMeta;
  ImageMetadata transferMeta{};
  transferMeta.layout = ImageLayout::TRANSFER_DST;
  transferMeta.ownership = ownership;
  transferMeta.stage = PipelineStage::TRANSFER;
  transferMeta.access = ResourceAccess::TRANSFER_WRITE;
  createImageBarrier(context, spec.image, imageMeta, transferMeta,
                     imageBarrier);
  dependencyInfo.pImageMemoryBarriers = &imageBarrier;
  dependencyInfo.imageMemoryBarrierCount = 1;
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

inline void imageDataCopyPlaceFinalBarrier(
    LContext context, const ImageDataCopySpecification &spec,
    QueueOwnershipState ownership, VkCommandBuffer commandBuffer) {
  VkBufferMemoryBarrier2 bufferBarrier{};
  VkImageMemoryBarrier2 imageBarrier{};

  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pBufferMemoryBarriers = nullptr;
  dependencyInfo.bufferMemoryBarrierCount = 0;
  dependencyInfo.pImageMemoryBarriers = nullptr;
  dependencyInfo.imageMemoryBarrierCount = 0;

  BufferMetadata stagingMeta{};
  stagingMeta.stage = PipelineStage::TRANSFER;
  stagingMeta.access = ResourceAccess::TRANSFER_READ;
  stagingMeta.ownership = ownership;

  ImageMetadata transferMeta{};
  transferMeta.layout = ImageLayout::TRANSFER_DST;
  transferMeta.stage = PipelineStage::TRANSFER;
  transferMeta.access = ResourceAccess::TRANSFER_WRITE;
  transferMeta.ownership = ownership;

  if ((QueueOwnershipState)spec.desiredStagingBufferOwnership != ownership) {
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
    createBufferBarrier(context, spec.stagingBuffer, stagingMeta,
                        finalSourceMeta, bufferBarrier);
    stagingMeta = finalSourceMeta;
    dependencyInfo.pBufferMemoryBarriers = &bufferBarrier;
    dependencyInfo.bufferMemoryBarrierCount = 1;
  }

  if (spec.outStagingBufferMeta) {
    *spec.outStagingBufferMeta = stagingMeta;
  }

  if (spec.desiredImageOwnership == QueueOwnership::GRAPHICS ||
      spec.desiredImageLayout != transferMeta.layout) {
    ImageMetadata outMeta;
    outMeta.layout = spec.desiredImageLayout;
    if (spec.desiredImageOwnership == QueueOwnership::GRAPHICS) {
      outMeta.ownership = QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS;
    } else {
      outMeta.ownership = QueueOwnershipState::TRANSFER;
    }
    outMeta.stage = PipelineStage::BOTTOM_OF_PIPE;
    outMeta.access = ResourceAccess::NONE;
    createImageBarrier(context, spec.image, transferMeta, outMeta,
                       imageBarrier);
    if (spec.outImageMeta) {
      *spec.outImageMeta = outMeta;
    }
    dependencyInfo.pImageMemoryBarriers = &imageBarrier;
    dependencyInfo.imageMemoryBarrierCount = 1;
  } else {
    if (spec.outImageMeta) {
      *spec.outImageMeta = transferMeta;
    }
  }

  if (dependencyInfo.imageMemoryBarrierCount > 0 ||
      dependencyInfo.bufferMemoryBarrierCount > 0) {
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
  }
}

} // namespace

void copyImageData(LContext context, const ImageDataCopySpecification &spec) {
  VkCommandPool commandPool;
  VkQueue queue;
  QueueOwnershipState ownership;
  getResourcePairPoolAndQueue(context, spec.imageMeta.ownership,
                              spec.stagingBufferMeta.ownership, commandPool,
                              queue, ownership);
  if (commandPool == VK_NULL_HANDLE) {
    return;
  }

  VkCommandBuffer commandBuffer =
      allocAndBeginTmpCmdBuffer(context, commandPool);

  imageDataCopyPlaceInitialBarrier(context, spec, ownership, commandBuffer);

  // Copy buffer to image
  VkBufferImageCopy region{};
  region.bufferOffset = spec.bufferOffset;
  region.bufferRowLength = 0;   // width
  region.bufferImageHeight = 0; // height
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {(int)spec.offsetX, (int)spec.offsetY, 0};
  region.imageExtent = {spec.width, spec.height, 1};

  vkCmdCopyBufferToImage(commandBuffer, spec.stagingBuffer->buffer,
                         spec.image->image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  imageDataCopyPlaceFinalBarrier(context, spec, ownership, commandBuffer);

  vkEndCommandBuffer(commandBuffer);

  // Submit command buffer
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(context->transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(context->transferQueue);

  // Cleanup
  vkFreeCommandBuffers(context->device, context->transferCommandPool, 1,
                       &commandBuffer);
}

namespace {

void imageCopyPlaceInitialBarrier(LContext context, Image source,
                                  const ImageMetadata &sourceMeta,
                                  Image destination,
                                  const ImageMetadata &destinationMeta,
                                  QueueOwnershipState ownership,
                                  VkCommandBuffer commandBuffer) {

  VkImageMemoryBarrier2 barriers[2];
  uint32_t barrierCount = 0;
  ImageMetadata transferSourceMeta{};
  transferSourceMeta.layout = ImageLayout::TRANSFER_SRC;
  transferSourceMeta.stage = PipelineStage::TRANSFER;
  transferSourceMeta.access = ResourceAccess::TRANSFER_READ;
  transferSourceMeta.ownership = ownership;
  if (transferSourceMeta.layout != sourceMeta.layout ||
      transferSourceMeta.stage != sourceMeta.stage ||
      transferSourceMeta.access != sourceMeta.access ||
      transferSourceMeta.ownership != ownership) {
    createImageBarrier(context, source, sourceMeta, transferSourceMeta,
                       barriers[barrierCount++]);
  }

  ImageMetadata transferDestinationMeta{};
  transferDestinationMeta.layout = ImageLayout::TRANSFER_DST;
  transferDestinationMeta.stage = PipelineStage::TRANSFER;
  transferDestinationMeta.access = ResourceAccess::TRANSFER_WRITE;
  transferDestinationMeta.ownership = ownership;
  createImageBarrier(context, destination, destinationMeta,
                     transferDestinationMeta, barriers[barrierCount++]);

  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pImageMemoryBarriers = barriers;
  dependencyInfo.imageMemoryBarrierCount = barrierCount;
  vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

void imageCopyPlaceFinalBarrier(
    LContext context, Image source, ImageMetadata *outSourceMeta,
    QueueOwnership desiredSrcOwnership, ImageLayout desiredSrcLayout,
    Image destination, ImageMetadata *outDestinationMeta,
    QueueOwnership desiredDstOwnership, ImageLayout desiredDstLayout,
    QueueOwnershipState ownership, VkCommandBuffer commandBuffer) {
  ImageMetadata sourceMeta{};
  sourceMeta.layout = ImageLayout::TRANSFER_SRC;
  sourceMeta.stage = PipelineStage::TRANSFER;
  sourceMeta.access = ResourceAccess::TRANSFER_READ;
  sourceMeta.ownership = ownership;

  ImageMetadata destinationMeta{};
  destinationMeta.layout = ImageLayout::TRANSFER_DST;
  destinationMeta.stage = PipelineStage::TRANSFER;
  destinationMeta.access = ResourceAccess::TRANSFER_WRITE;
  destinationMeta.ownership = ownership;

  if (desiredSrcLayout == ImageLayout::UNDEFINED) {
    sourceMeta.layout = ImageLayout::UNDEFINED;
  }

  if (desiredDstLayout == ImageLayout::UNDEFINED) {
    destinationMeta.layout = ImageLayout::UNDEFINED;
  }

  VkImageMemoryBarrier2 barriers[2];
  uint32_t barrierCount = 0;
  if ((QueueOwnershipState)desiredSrcOwnership != ownership ||
      desiredSrcLayout != sourceMeta.layout) {
    ImageMetadata finalSourceMeta{};
    finalSourceMeta.access = ResourceAccess::NONE;
    finalSourceMeta.layout = desiredSrcLayout;
    if (ownership == QueueOwnershipState::GRAPHICS) {
      finalSourceMeta.stage = PipelineStage::TRANSFER;
      finalSourceMeta.ownership =
          QueueOwnershipState::GRAPHICS_RELEASED_TO_TRANSFER;
    } else {
      finalSourceMeta.stage = PipelineStage::BOTTOM_OF_PIPE;
      finalSourceMeta.ownership =
          QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS;
    }
    createImageBarrier(context, source, sourceMeta, finalSourceMeta,
                       barriers[barrierCount++]);
    sourceMeta = finalSourceMeta;
  }

  if ((QueueOwnershipState)desiredDstOwnership != ownership ||
      desiredDstLayout != destinationMeta.layout) {
    ImageMetadata finalDestinationMeta{};
    finalDestinationMeta.access = ResourceAccess::NONE;
    finalDestinationMeta.layout = desiredDstLayout;
    if (ownership == QueueOwnershipState::GRAPHICS) {
      finalDestinationMeta.stage = PipelineStage::TRANSFER;
      finalDestinationMeta.ownership =
          QueueOwnershipState::GRAPHICS_RELEASED_TO_TRANSFER;
    } else {
      finalDestinationMeta.stage = PipelineStage::BOTTOM_OF_PIPE;
      finalDestinationMeta.ownership =
          QueueOwnershipState::TRANSFER_RELEASED_TO_GRAPHICS;
    }

    createImageBarrier(context, destination, destinationMeta,
                       finalDestinationMeta, barriers[barrierCount++]);
    destinationMeta = finalDestinationMeta;
  }

  if (barrierCount > 0) {
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pImageMemoryBarriers = barriers;
    dependencyInfo.imageMemoryBarrierCount = barrierCount;
    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
  }

  if (outSourceMeta) {
    *outSourceMeta = sourceMeta;
  }
  if (outDestinationMeta) {
    *outDestinationMeta = destinationMeta;
  }
}

} // namespace

void copyImage(LContext context, const ImageCopySpecification &spec) {
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

  imageCopyPlaceInitialBarrier(context, spec.source, spec.sourceMeta,
                               spec.destination, spec.destinationMeta,
                               ownership, commandBuffer);

  copyImage(commandBuffer, spec.source->image, spec.destination->image,
            spec.srcX, spec.srcY, spec.dstX, spec.dstY, spec.width,
            spec.height);

  imageCopyPlaceFinalBarrier(context, spec.source, spec.outSourceMeta,
                             spec.desiredSrcOwnership, spec.desiredSrcLayout,
                             spec.destination, spec.outDestinationMeta,
                             spec.desiredDstOwnership, spec.desiredDstLayout,
                             ownership, commandBuffer);

  vkEndCommandBuffer(commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);

  vkFreeCommandBuffers(context->device, commandPool, 1, &commandBuffer);
}

void blitImage(LContext context, const ImageBlitSpecification &spec) {
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

  ImageMetadata transferMeta;
  transferMeta.layout = ImageLayout::TRANSFER_SRC;
  transferMeta.ownership = QueueOwnershipState::GRAPHICS;
  transferMeta.stage = PipelineStage::TRANSFER;
  transferMeta.access = ResourceAccess::TRANSFER_READ;

  imageCopyPlaceInitialBarrier(context, spec.source, spec.sourceMeta,
                               spec.destination, spec.destinationMeta,
                               ownership, commandBuffer);

  blitImage(commandBuffer, spec.source->image, spec.destination->image,
            spec.source->width, spec.source->height, spec.destination->width,
            spec.destination->height, getVulkanFilter(spec.filter));

  imageCopyPlaceFinalBarrier(context, spec.source, spec.outSourceMeta,
                             spec.desiredSrcOwnership, spec.desiredSrcLayout,
                             spec.destination, spec.outDestinationMeta,
                             spec.desiredDstOwnership, spec.desiredDstLayout,
                             ownership, commandBuffer);

  vkEndCommandBuffer(commandBuffer);

  // Submit command buffer
  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &commandBuffer;

  vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(context->graphicsQueue);

  // Cleanup
  vkFreeCommandBuffers(context->device, context->graphicsCommandPool, 1,
                       &commandBuffer);
}

void transitionImage(LContext context, Image image, ImageMetadata meta,
                     ImageMetadata outMeta) {
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

  VkImageMemoryBarrier2 imageBarrier{};
  imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  createImageBarrier(context, image, meta, outMeta, imageBarrier);

  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pImageMemoryBarriers = &imageBarrier;
  dependencyInfo.imageMemoryBarrierCount = 1;
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
