#include "draw-processor.h"
#include "helpers.h"
#include "types.h"

#include <vulkan/vulkan.h>

#include <print>

namespace svet::renderer {

DrawProcessor createDrawProcessor(LContext context) {
  auto drawProcessor = new DrawProcessorT;

  VkCommandPoolCreateInfo commandPoolInfo{};
  commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolInfo.queueFamilyIndex = context->graphicsQueueFamilyIndex;
  vkCreateCommandPool(context->device, &commandPoolInfo, nullptr,
                      &drawProcessor->commandPool);

  VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
  commandBufferAllocateInfo.sType =
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandBufferCount = 1;
  commandBufferAllocateInfo.commandPool = drawProcessor->commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(context->device, &commandBufferAllocateInfo,
                           &drawProcessor->primaryCommandBuffer);

  drawProcessor->secondaryCommandBuffers = nullptr;
  drawProcessor->secondaryCommandBufferCount = 0;

  return drawProcessor;
}

void destroyDrawProcessor(LContext context, DrawProcessor drawProcessor) {
  vkDestroyCommandPool(context->device, drawProcessor->commandPool, nullptr);
  delete drawProcessor;
}

void resetDrawProcessor(LContext context, DrawProcessor drawProcessor) {
  vkResetCommandPool(context->device, drawProcessor->commandPool, 0);

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  vkBeginCommandBuffer(drawProcessor->primaryCommandBuffer, &beginInfo);
}

void submitDrawProcessor(LContext context, DrawProcessor drawProcessor) {
  vkEndCommandBuffer(drawProcessor->primaryCommandBuffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &drawProcessor->primaryCommandBuffer;

  vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);

  vkQueueWaitIdle(context->graphicsQueue);
}

void cmdBeginRenderPass(DrawProcessor drawProcessor, RenderPass renderPass) {
  VkRenderingInfo renderingInfo{};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.pColorAttachments = renderPass->colorAttachmentInfos;
  renderingInfo.colorAttachmentCount = renderPass->renderTargetCount;
  renderingInfo.pDepthAttachment =
      renderPass->depthTarget ? &renderPass->depthAttachmentInfo : nullptr;
  renderingInfo.layerCount = 1;
  renderingInfo.renderArea = {{0, 0}, {renderPass->width, renderPass->height}};
  renderingInfo.viewMask = 0;
  vkCmdBeginRendering(drawProcessor->primaryCommandBuffer, &renderingInfo);
}

void cmdEndRenderPass(DrawProcessor drawProcessor) {
  vkCmdEndRendering(drawProcessor->primaryCommandBuffer);
}

void cmdBindPipeline(DrawProcessor drawProcessor, Pipeline pipeline) {
  VkPipelineBindPoint bindPoint;
  if (pipeline->isGraphics)
    bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  else
    bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

  vkCmdBindPipeline(drawProcessor->primaryCommandBuffer, bindPoint,
                    pipeline->pipeline);

  drawProcessor->boundPipeline = pipeline;
}

void cmdBindDescriptorSet(DrawProcessor drawProcessor,
                          DescriptorSetBinding binding) {
  cmdBindDescriptorSets(drawProcessor, &binding, 1);
}

void cmdBindDescriptorSet(DrawProcessor drawProcessor, DescriptorSet set,
                          uint32_t binding) {

  cmdBindDescriptorSet(drawProcessor, {set, binding});
}

void cmdBindDescriptorSets(DrawProcessor drawProcessor,
                           DescriptorSetBinding *bindings,
                           uint32_t bindingCount) {
  Pipeline pipeline = drawProcessor->boundPipeline;
  if (pipeline == VK_NULL_HANDLE) {
    std::println("[Vulkan] A pipeline must be bound to process descriptor "
                 "set binding");
    return;
  }
  VkPipelineBindPoint bindPoint;
  if (pipeline->isGraphics)
    bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  else
    bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;

  uint32_t remainingBindings = bindingCount;
  while (remainingBindings > 0) {
    uint32_t baseIndex = bindingCount - remainingBindings;
    uint32_t firstBinding = bindings[baseIndex].binding;
    if (firstBinding == UINT32_MAX)
      firstBinding = 0;

    // TODO: Could add dynamic offsets for flexibility
    uint32_t currentCount = 0;
    VkDescriptorSet descriptorSets[4];
    for (int i = 0; i < remainingBindings; ++i) {
      descriptorSets[i] = bindings[baseIndex + i].set->descriptorSet;
      uint32_t binding = bindings[baseIndex + i].binding;
      if (firstBinding + i != binding && binding != UINT32_MAX) {
        break;
      }
      ++currentCount;
    }
    remainingBindings -= currentCount;

    vkCmdBindDescriptorSets(drawProcessor->primaryCommandBuffer, bindPoint,
                            pipeline->layout, firstBinding, currentCount,
                            descriptorSets, 0, nullptr);
  }
}

void cmdPushConstant(DrawProcessor drawProcessor, void *data, uint32_t size,
                     uint32_t offset, ShaderVisibility vis) {
  Pipeline pipeline = drawProcessor->boundPipeline;
  if (pipeline == VK_NULL_HANDLE) {
    std::println("[Vulkan] A pipeline must be bound to process descriptor "
                 "set binding");
    return;
  }

  vkCmdPushConstants(drawProcessor->primaryCommandBuffer, pipeline->layout,
                     getShaderStageFlags(vis), offset, size, data);
}

void cmdImageBarrier(LContext context, DrawProcessor drawProcessor,
                     ImageBarrier barrier) {
  cmdImageBarriers(context, drawProcessor, &barrier, 1);
}

void cmdImageBarrier(LContext context, DrawProcessor drawProcessor, Image image,
                     ImageMetadata fromMeta, ImageMetadata toMeta) {
  cmdImageBarrier(context, drawProcessor, {image, fromMeta, toMeta});
}

void cmdImageBarriers(LContext context, DrawProcessor drawProcessor,
                      ImageBarrier *imageBarriers, uint32_t barrierCount) {
  const uint32_t maxBarriersAtOnce = 16;
  uint32_t remainingCount = barrierCount;
  for (uint32_t k = 0;
       k < (barrierCount + maxBarriersAtOnce - 1) / maxBarriersAtOnce; ++k) {
    VkImageMemoryBarrier2 barriers[maxBarriersAtOnce];

    uint32_t actualCount = std::min(remainingCount, maxBarriersAtOnce);
    for (int i = 0; i < actualCount; ++i) {
      const ImageBarrier &imageBarrier =
          imageBarriers[k * maxBarriersAtOnce + i];

      createImageBarrier(context, imageBarrier.image, imageBarrier.fromMeta,
                         imageBarrier.toMeta, barriers[i]);
    }

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pImageMemoryBarriers = barriers;
    dependencyInfo.imageMemoryBarrierCount = actualCount;

    vkCmdPipelineBarrier2(drawProcessor->primaryCommandBuffer, &dependencyInfo);

    remainingCount -= actualCount;
  }
}

void cmdBufferBarrier(LContext context, DrawProcessor drawProcessor,
                      BufferBarrier barrier) {
  cmdBufferBarriers(context, drawProcessor, &barrier, 1);
}

void cmdBufferBarrier(LContext context, DrawProcessor drawProcessor,
                      Buffer buffer, BufferMetadata fromMeta,
                      BufferMetadata toMeta) {
  cmdBufferBarrier(context, drawProcessor, {buffer, fromMeta, toMeta});
}

void cmdBufferBarriers(LContext context, DrawProcessor drawProcessor,
                       BufferBarrier *bufferBarriers, uint32_t barrierCount) {
  const uint32_t maxBarriersAtOnce = 16;
  uint32_t remainingCount = barrierCount;
  for (uint32_t k = 0;
       k < (barrierCount + maxBarriersAtOnce - 1) / maxBarriersAtOnce; ++k) {
    VkBufferMemoryBarrier2 barriers[maxBarriersAtOnce];

    uint32_t actualCount = std::min(remainingCount, maxBarriersAtOnce);
    for (int i = 0; i < actualCount; ++i) {
      const BufferBarrier &bufferBarrier =
          bufferBarriers[k * maxBarriersAtOnce + i];

      createBufferBarrier(context, bufferBarrier.buffer, bufferBarrier.fromMeta,
                          bufferBarrier.toMeta, barriers[i]);
    }

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.pBufferMemoryBarriers = barriers;
    dependencyInfo.bufferMemoryBarrierCount = actualCount;

    vkCmdPipelineBarrier2(drawProcessor->primaryCommandBuffer, &dependencyInfo);

    remainingCount -= actualCount;
  }
}

void cmdBindVertexAttrib(DrawProcessor drawProcessor, VertexBinding binding) {
  cmdBindVertexAttribs(drawProcessor, &binding, 1);
}

void cmdBindVertexAttrib(DrawProcessor drawProcessor, Buffer buffer,
                         uint32_t binding, uint32_t offset) {
  cmdBindVertexAttrib(drawProcessor, {buffer, binding, offset});
}

void cmdBindVertexAttribs(DrawProcessor drawProcessor, VertexBinding *bindings,
                          uint32_t bindingCount) {
  if (bindingCount > 16) {
    // TODO: Could quietly process this as multiple vkCmd calls
    std::println(
        "[Vulkan] A maximum of 16 vertex buffers can be bound at once");
    return;
  }
  uint32_t firstBinding = bindings[0].binding;
  if (firstBinding == UINT32_MAX)
    firstBinding = 0;

  VkBuffer buffers[16];
  VkDeviceSize offsets[16];
  for (int i = 0; i < bindingCount; ++i) {
    buffers[i] = bindings[i].buffer->buffer;
    offsets[i] = bindings[i].offset;
    uint32_t binding = bindings[i].binding;
    if (firstBinding + i != binding && binding != UINT32_MAX) {
      // TODO: Could quietly process this as multiple vkCmd calls
      std::println("[Vulkan] Bulked vertex bindings must have consecutive bind "
                   "points or be UINT32_MAX");
      return;
    }
  }

  vkCmdBindVertexBuffers(drawProcessor->primaryCommandBuffer, firstBinding,
                         bindingCount, buffers, offsets);
}

void cmdBindIndexBuffer(DrawProcessor drawProcessor, Buffer buffer,
                        uint32_t inOffset) {
  VkDeviceSize offset = inOffset;
  vkCmdBindIndexBuffer(drawProcessor->primaryCommandBuffer, buffer->buffer,
                       offset, VK_INDEX_TYPE_UINT32);
}

void cmdDrawCall(DrawProcessor drawProcessor, uint32_t elementCount,
                 uint32_t instanceCount, bool indexed, uint32_t firstElement,
                 uint32_t firstInstance, uint32_t vertexOffset) {
  if (indexed) {
    vkCmdDrawIndexed(drawProcessor->primaryCommandBuffer, elementCount,
                     instanceCount, firstElement, vertexOffset, firstInstance);
  } else {
    vkCmdDraw(drawProcessor->primaryCommandBuffer, elementCount, instanceCount,
              firstElement, firstInstance);
  }
}

void cmdIndirectDrawCall(DrawProcessor drawProcessor, Buffer buffer,
                         uint32_t offset, uint32_t drawCount, uint32_t stride,
                         bool indexed) {
  if (indexed) {
    vkCmdDrawIndexedIndirect(drawProcessor->primaryCommandBuffer,
                             buffer->buffer, offset, drawCount, stride);
  } else {
    vkCmdDrawIndirect(drawProcessor->primaryCommandBuffer, buffer->buffer,
                      offset, drawCount, stride);
  }
}

void cmdIndirectDrawCall(DrawProcessor drawProcessor, Buffer buffer,
                         uint32_t offset, Buffer countBuffer,
                         uint32_t countBufferOffset, uint32_t maxDrawCount,
                         uint32_t stride, bool indexed) {
  if (indexed) {
    vkCmdDrawIndexedIndirectCount(drawProcessor->primaryCommandBuffer,
                                  buffer->buffer, offset, countBuffer->buffer,
                                  countBufferOffset, maxDrawCount, stride);
  } else {
    vkCmdDrawIndirectCount(drawProcessor->primaryCommandBuffer, buffer->buffer,
                           offset, countBuffer->buffer, countBufferOffset,
                           maxDrawCount, stride);
  }
}

void cmdDispatch(DrawProcessor drawProcessor, uint32_t groupCountX,
                 uint32_t groupCountY, uint32_t groupCountZ) {
  vkCmdDispatch(drawProcessor->primaryCommandBuffer, groupCountX, groupCountY,
                groupCountZ);
}

void cmdBlit(DrawProcessor drawProcessor, Image from, Image to,
             SampleFilter filter) {
  blitImage(drawProcessor->primaryCommandBuffer, from->image, to->image,
            from->width, from->height, to->width, to->height,
            getVulkanFilter(filter));
}

} // namespace svet::renderer
