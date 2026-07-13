#pragma once
#include "context.h"
#include "descriptor.h"
#include "enums.h"
#include "shader.h"

#include <cstdint>

namespace svet::renderer {

using PipelineLayout = struct PipelineLayoutT *;
using Pipeline = struct PipelineT *;

//
// PIPELINE LAYOUT
//
struct PushConstant {
  ShaderVisibility visibility;
  uint32_t offset;
  uint32_t size;
};
PipelineLayout createPipelineLayout(LContext context, DescriptorSetLayout *sets,
                                    uint32_t setCount,
                                    PushConstant *pushConstants,
                                    uint32_t constantCount);
void destroyPipelineLayout(LContext context, PipelineLayout pipelineLayout);

//
// PIPELINE
//
struct VertexAttributeDescription {
  uint32_t location; // Shader attribute location (0, 1, 2, etc.)
  uint32_t offset;   // Offset within the buffer
  uint32_t binding;  // Which buffer binding point (0, 1, 2, etc.)
  VertexFormat format;
};
struct VertexBufferBinding {
  uint32_t bindingIndex;
  uint32_t stride;
  VertexInputRate inputRate;
};
struct RenderTargetBlend {
  bool allow;
  ColorComponent colorWriteMask;
  BlendOp colorOp;
  BlendFactor srcColorFactor;
  BlendFactor dstColorFactor;
  BlendOp alphaOp;
  BlendFactor srcAlphaFactor;
  BlendFactor dstAlphaFactor;
};
struct GraphicsPipelineSpecification {
  uint32_t width;
  uint32_t height;
  Shader vertexShader;
  Shader fragmentShader;
  PipelineLayout pipelineLayout;
  const VertexAttributeDescription *attributes;
  uint32_t attributeCount;
  const VertexBufferBinding *vertexBindings;
  uint32_t vertexBindingCount;
  DrawMode drawMode;
  const PixelFormat *renderTargetFormats;
  RenderTargetBlend *renderTargetBlends;
  uint32_t renderTargetCount;
  const PixelFormat *depthPixelFormat;
  FrontFaceWind frontFaceWind;
  FaceCulling faceCulling;
  uint32_t multiSampleCount;
  float depthMin;
  float depthMax;
  bool flipViewportY;
  bool enableDepthTest;
  bool enableDepthWrite;
  bool enableAlphaToCoverage;
};
Pipeline createGraphicsPipeline(LContext context,
                                const GraphicsPipelineSpecification &spec);
struct ComputePipelineSpecification {
  Shader computeShader;
  PipelineLayout pipelineLayout;
};
Pipeline createComputePipeline(LContext context,
                               const ComputePipelineSpecification &spec);
void destroyPipeline(LContext context, Pipeline pipeline);

} // namespace svet::renderer
