#include "swapchain.h"
#include "helpers.h"
#include "types.h"

#include <vulkan/vulkan.h>

#include <chrono>
#include <print>
#include <thread>

namespace svet::renderer {

Swapchain createSwapchain(LContext context,
                          const SwapchainSpecification &spec) {
  auto swapchain = new SwapchainT;

  swapchain->stabilizeFramerate = spec.stabilizeFramerate;

  VkSurfaceFormatKHR surfaceFormat =
      chooseSwapchainFormat(context->physicalDevice, context->surface);
  VkPresentModeKHR presentMode = chooseSwapchainPresentMode(
      context->physicalDevice, context->surface, spec.vSync);
  VkExtent2D extent = chooseSwapchainExtent(
      context->physicalDevice, context->surface, spec.width, spec.height);
  swapchain->width = extent.width;
  swapchain->height = extent.height;

  VkSurfaceCapabilitiesKHR capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physicalDevice,
                                            context->surface, &capabilities);

  uint32_t imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 &&
      imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR swapchainInfo{};
  swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchainInfo.surface = context->surface;
  swapchainInfo.minImageCount = imageCount;
  swapchainInfo.imageFormat = surfaceFormat.format;
  swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
  swapchainInfo.imageExtent = extent;
  swapchainInfo.imageArrayLayers = 1;
  swapchainInfo.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainInfo.queueFamilyIndexCount = 0;
  swapchainInfo.pQueueFamilyIndices = nullptr;
  swapchainInfo.preTransform = capabilities.currentTransform;
  swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainInfo.presentMode = presentMode;
  swapchainInfo.clipped = VK_TRUE;
  swapchainInfo.oldSwapchain =
      spec.retiredSwapchain ? spec.retiredSwapchain->swapchain : VK_NULL_HANDLE;

  swapchain->format = surfaceFormat.format;

  auto result = vkCreateSwapchainKHR(context->device, &swapchainInfo, nullptr,
                                     &swapchain->swapchain);

  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to create swapchain: {}", (int)result);
    delete swapchain;
    return nullptr;
  }

  result = vkGetSwapchainImagesKHR(context->device, swapchain->swapchain,
                                   &swapchain->imageCount, nullptr);

  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to get swapchain images: {}", (int)result);
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  swapchain->images = new VkImage[swapchain->imageCount];
  vkGetSwapchainImagesKHR(context->device, swapchain->swapchain,
                          &swapchain->imageCount, swapchain->images);

  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Failed to get swapchain images: {}", (int)result);
    delete[] swapchain->images;
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  swapchain->imageViews = new VkImageView[swapchain->imageCount];

  for (uint32_t i = 0; i < swapchain->imageCount; i++) {
    VkImageViewCreateInfo imageViewInfo{};
    imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewInfo.image = swapchain->images[i];
    imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewInfo.format = surfaceFormat.format;
    imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageViewInfo.subresourceRange.baseMipLevel = 0;
    imageViewInfo.subresourceRange.levelCount = 1;
    imageViewInfo.subresourceRange.baseArrayLayer = 0;
    imageViewInfo.subresourceRange.layerCount = 1;

    result = vkCreateImageView(context->device, &imageViewInfo, nullptr,
                               &swapchain->imageViews[i]);
    if (result != VK_SUCCESS) {
      std::println(stderr, "[Vulkan] Failed to create image view {}: {}", i,
                   (int)result);
      for (uint32_t j = 0; j < i; j++) {
        vkDestroyImageView(context->device, swapchain->imageViews[j], nullptr);
      }
      delete[] swapchain->images;
      delete[] swapchain->imageViews;
      vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
      delete swapchain;
      return nullptr;
    }
  }

  VkCommandPoolCreateInfo commandPoolInfo{};
  commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolInfo.queueFamilyIndex = context->graphicsQueueFamilyIndex;
  commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  result = vkCreateCommandPool(context->device, &commandPoolInfo, nullptr,
                               &swapchain->commandPool);

  if (result != VK_SUCCESS) {
    std::println(stderr, "[Vulkan] Failed to create swapchain command pool: {}",
                 (int)result);
    for (uint32_t i = 0; i < swapchain->imageCount; i++) {
      vkDestroyImageView(context->device, swapchain->imageViews[i], nullptr);
    }
    delete[] swapchain->images;
    delete[] swapchain->imageViews;
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  VkCommandBufferAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocateInfo.commandPool = swapchain->commandPool;
  allocateInfo.commandBufferCount = 1;
  result = vkAllocateCommandBuffers(context->device, &allocateInfo,
                                    &swapchain->commandBuffer);

  if (result != VK_SUCCESS) {
    std::println(stderr,
                 "[Vulkan] Failed to allocate swapchain command buffer: {}",
                 (int)result);
    vkDestroyCommandPool(context->device, swapchain->commandPool, nullptr);
    for (uint32_t i = 0; i < swapchain->imageCount; i++) {
      vkDestroyImageView(context->device, swapchain->imageViews[i], nullptr);
    }
    delete[] swapchain->images;
    delete[] swapchain->imageViews;
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  swapchain->presentQueue = context->graphicsQueue;

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  result = vkCreateSemaphore(context->device, &semaphoreInfo, nullptr,
                             &swapchain->blitSemaphore);

  if (result != VK_SUCCESS) {
    std::println(stderr,
                 "[Vulkan] Failed to create swapchain blit semaphore: {}",
                 (int)result);
    vkDestroyCommandPool(context->device, swapchain->commandPool, nullptr);
    for (uint32_t i = 0; i < swapchain->imageCount; i++) {
      vkDestroyImageView(context->device, swapchain->imageViews[i], nullptr);
    }
    delete[] swapchain->images;
    delete[] swapchain->imageViews;
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  vkCreateSemaphore(context->device, &semaphoreInfo, nullptr,
                    &swapchain->acquireSemaphore);

  if (result != VK_SUCCESS) {
    std::println(stderr,
                 "[Vulkan] Failed to create swapchain acquire semaphore: {}",
                 (int)result);
    vkDestroySemaphore(context->device, swapchain->blitSemaphore, nullptr);
    vkDestroyCommandPool(context->device, swapchain->commandPool, nullptr);
    for (uint32_t i = 0; i < swapchain->imageCount; i++) {
      vkDestroyImageView(context->device, swapchain->imageViews[i], nullptr);
    }
    delete[] swapchain->images;
    delete[] swapchain->imageViews;
    vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
    delete swapchain;
    return nullptr;
  }

  return swapchain;
}

void present(LContext context, Swapchain swapchain, Image image,
             ImageMetadata meta, ImageMetadata *outMeta,
             SampleFilter blitFilter) {
  if (meta.ownership != QueueOwnershipState::GRAPHICS) {
    std::println("[Vulkan] Presenting via a different queue than graphics is "
                 "not supported");
    return;
  }
  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      context->device, swapchain->swapchain, UINT64_MAX,
      swapchain->acquireSemaphore, VK_NULL_HANDLE, &imageIndex);

  if (result != VK_SUCCESS) {
    std::println("[Vulkan] Swapchain could not acquire image");
    return;
  }

  VkImage swapImage = swapchain->images[imageIndex];

  vkResetCommandPool(context->device, swapchain->commandPool, 0);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(swapchain->commandBuffer, &beginInfo);

  VkImageMemoryBarrier2 barriers[2];
  ImageMetadata transferSrcMeta;
  transferSrcMeta.layout = ImageLayout::TRANSFER_SRC;
  transferSrcMeta.ownership = QueueOwnershipState::GRAPHICS;
  transferSrcMeta.stage = PipelineStage::TRANSFER;
  transferSrcMeta.access = ResourceAccess::TRANSFER_READ;
  createImageBarrier(context, image, meta, transferSrcMeta, barriers[0]);

  if (outMeta) {
    *outMeta = transferSrcMeta;
  }

  ImageMetadata sourceMeta;
  sourceMeta.layout = ImageLayout::UNDEFINED;
  sourceMeta.ownership = QueueOwnershipState::GRAPHICS;
  sourceMeta.stage = PipelineStage::TOP_OF_PIPE;
  sourceMeta.access = ResourceAccess::NONE;

  ImageMetadata transferDstMeta;
  transferDstMeta.layout = ImageLayout::TRANSFER_DST;
  transferDstMeta.ownership = QueueOwnershipState::GRAPHICS;
  transferDstMeta.stage = PipelineStage::TRANSFER;
  transferDstMeta.access = ResourceAccess::TRANSFER_WRITE;
  ImageT swapImageWrapper{};
  swapImageWrapper.image = swapImage;
  swapImageWrapper.isDepth = false;
  createImageBarrier(context, &swapImageWrapper, sourceMeta, transferDstMeta,
                     barriers[1]);

  VkDependencyInfo dependencyInfo{};
  dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  dependencyInfo.pImageMemoryBarriers = barriers;
  dependencyInfo.imageMemoryBarrierCount = 2;
  vkCmdPipelineBarrier2(swapchain->commandBuffer, &dependencyInfo);

  // TODO: MSAA mismatch also requires blit
  if (image->width == swapchain->width && image->height == swapchain->height &&
      image->format == swapchain->format) {
    copyImage(swapchain->commandBuffer, image->image, swapImage,
              swapchain->width, swapchain->height);
  } else {
    blitImage(swapchain->commandBuffer, image->image, swapImage, image->width,
              image->height, swapchain->width, swapchain->height,
              getVulkanFilter(blitFilter));
  }

  ImageMetadata presentMeta;
  presentMeta.layout = ImageLayout::PRESENT;
  presentMeta.ownership = QueueOwnershipState::GRAPHICS;
  presentMeta.stage = PipelineStage::BOTTOM_OF_PIPE;
  presentMeta.access = ResourceAccess::NONE;
  createImageBarrier(context, &swapImageWrapper, transferDstMeta, presentMeta,
                     barriers[0]);
  dependencyInfo.imageMemoryBarrierCount = 1;
  vkCmdPipelineBarrier2(swapchain->commandBuffer, &dependencyInfo);

  vkEndCommandBuffer(swapchain->commandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.pCommandBuffers = &swapchain->commandBuffer;
  submitInfo.commandBufferCount = 1;
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_TRANSFER_BIT};
  submitInfo.pWaitSemaphores = &swapchain->acquireSemaphore;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = &swapchain->blitSemaphore;
  submitInfo.signalSemaphoreCount = 1;
  vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

  VkPresentInfoKHR presentInfo{};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain->swapchain;
  presentInfo.pImageIndices = &imageIndex;
  presentInfo.pWaitSemaphores = &swapchain->blitSemaphore;
  presentInfo.waitSemaphoreCount = 1;

  // TODO: This should be capped at monitor refresh rate if VSync is on.
  if (swapchain->stabilizeFramerate != 0) {
    const uint64_t target = 1000000000 / swapchain->stabilizeFramerate;
    uint64_t frameTime =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
    auto delta = frameTime - swapchain->lastFrameTime;
    // Sleep if we have time until the end of the frame. This makes sure that
    // all frames are equally long
    if (delta < target) {
      auto delay = std::chrono::nanoseconds(target - delta);
      std::this_thread::sleep_for(delay);
    }

    // After potentially sleeping, remember the time.
    swapchain->lastFrameTime =
        std::chrono::high_resolution_clock::now().time_since_epoch().count();
  }
  vkQueuePresentKHR(swapchain->presentQueue, &presentInfo);

  vkQueueWaitIdle(swapchain->presentQueue);
}

void destroySwapchain(LContext context, Swapchain swapchain) {
  vkDestroySemaphore(context->device, swapchain->acquireSemaphore, nullptr);
  vkDestroySemaphore(context->device, swapchain->blitSemaphore, nullptr);
  vkDestroyCommandPool(context->device, swapchain->commandPool, nullptr);
  for (uint32_t i = 0; i < swapchain->imageCount; i++) {
    vkDestroyImageView(context->device, swapchain->imageViews[i], nullptr);
  }
  delete[] swapchain->imageViews;
  delete[] swapchain->images;
  vkDestroySwapchainKHR(context->device, swapchain->swapchain, nullptr);
  delete swapchain;
}

} // namespace svet::renderer
