#pragma once
#include "buffer.h"
#include "context.h"
#include "descriptor.h"
#include "enums.h"
#include "image.h"
#include "pipeline.h"
#include "render-pass.h"

#include <cstdint>

namespace svet::renderer {

using DrawProcessor = struct DrawProcessorT *;

struct ImageBarrier {
  Image image;
  ImageMetadata fromMeta;
  ImageMetadata toMeta;
};
struct BufferBarrier {
  Buffer buffer;
  BufferMetadata fromMeta;
  BufferMetadata toMeta;
};
struct DescriptorSetBinding {
  DescriptorSet set;
  uint32_t binding = UINT32_MAX;
};
struct VertexBinding {
  Buffer buffer;
  uint32_t binding = UINT32_MAX;
  uint32_t offset = 0;
};
DrawProcessor createDrawProcessor(LContext context);
void destroyDrawProcessor(LContext context, DrawProcessor drawProcessor);

void resetDrawProcessor(LContext context, DrawProcessor drawProcessor);
void submitDrawProcessor(LContext context, DrawProcessor drawProcessor);

void cmdBeginRenderPass(DrawProcessor drawProcessor, RenderPass renderPass);
void cmdEndRenderPass(DrawProcessor drawProcessor);
void cmdBindPipeline(DrawProcessor drawProcessor, Pipeline pipeline);
void cmdBindDescriptorSet(DrawProcessor drawProcessor,
                          DescriptorSetBinding binding);
void cmdBindDescriptorSet(DrawProcessor drawProcessor, DescriptorSet set,
                          uint32_t binding = UINT32_MAX);
void cmdBindDescriptorSets(DrawProcessor drawProcessor,
                           DescriptorSetBinding *bindings,
                           uint32_t bindingCount);
void cmdPushConstant(DrawProcessor drawProcessor, void *data, uint32_t size,
                     uint32_t offset, ShaderVisibility vis);
template <typename T>
inline void cmdPushConstant(DrawProcessor drawProcessor, T data,
                            ShaderVisibility vis, uint32_t offset = 0) {
  cmdPushConstant(drawProcessor, &data, sizeof(T), offset, vis);
}
void cmdImageBarrier(LContext context, DrawProcessor drawProcessor,
                     ImageBarrier barrier);
void cmdImageBarrier(LContext context, DrawProcessor drawProcessor, Image image,
                     ImageMetadata fromMeta, ImageMetadata toMeta);
void cmdImageBarriers(LContext context, DrawProcessor drawProcessor,
                      ImageBarrier *barriers, uint32_t barrierCount);
void cmdBufferBarrier(LContext context, DrawProcessor drawProcessor,
                      BufferBarrier barrier);
void cmdBufferBarrier(LContext context, DrawProcessor drawProcessor,
                      Buffer buffer, BufferMetadata fromMeta,
                      BufferMetadata toMeta);
void cmdBufferBarriers(LContext context, DrawProcessor drawProcessor,
                       BufferBarrier *barriers, uint32_t barrierCount);
void cmdBindVertexAttrib(DrawProcessor drawProcessor, VertexBinding binding);
void cmdBindVertexAttrib(DrawProcessor drawProcessor, Buffer buffer,
                         uint32_t binding = UINT32_MAX, uint32_t offset = 0);
void cmdBindVertexAttribs(DrawProcessor drawProcessor, VertexBinding *bindings,
                          uint32_t bindingCount);
void cmdBindIndexBuffer(DrawProcessor drawProcessor, Buffer buffer,
                        uint32_t offset = 0);
void cmdDrawCall(DrawProcessor drawProcessor, uint32_t elementCount,
                 uint32_t instanceCount, bool indexed,
                 uint32_t firstElement = 0, uint32_t firstInstance = 0,
                 uint32_t vertexOffset = 0);
void cmdIndirectDrawCall(DrawProcessor drawProcessor, Buffer buffer,
                         uint32_t offset, uint32_t drawCount, uint32_t stride,
                         bool indexed);
void cmdIndirectDrawCall(DrawProcessor drawProcessor, Buffer buffer,
                         uint32_t offset, Buffer countBuffer,
                         uint32_t countBufferOffset, uint32_t maxDrawCount,
                         uint32_t stride, bool indexed);
void cmdDispatch(DrawProcessor drawProcessor, uint32_t groupCountX,
                 uint32_t groupCountY, uint32_t groupCountZ = 1);
void cmdBlit(DrawProcessor drawProcessor, Image from, Image to,
             SampleFilter filter);

} // namespace svet::renderer
