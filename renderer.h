#pragma once
#include "enums.h"

#include <cstddef>
#include <cstdint>

struct SDL_Window;

//
// DATA
//

using LContext = struct LContextT *;
using Buffer = struct BufferT *;
using Shader = struct ShaderT *;
using PipelineLayout = struct PipelineLayoutT *;
using Pipeline = struct PipelineT *;
using DescriptorSetLayout = struct DescriptorSetLayoutT *;
using RenderPass = struct RenderPassT *;
using Image = struct ImageT *;
using Sampler = struct SamplerT *;
using Swapchain = struct SwapchainT *;
using DescriptorPool = struct DescriptorPoolT *;
using DescriptorSet = struct DescriptorSetT *;
using DrawProcessor = struct DrawProcessorT *;

//
// MANAGEMENT
//
struct RendererSpecification {
  bool allowValidation;
};
LContext init(SDL_Window *window, const RendererSpecification &spec);
void shutdown(LContext context);

//
// BUFFER
//
struct BufferMetadata {
  QueueOwnership ownership;
  PipelineStage stage;
  ResourceAccess access;
};
struct BufferSpecification {
  size_t size;
  BufferSource source;
  BufferConsumer consumer;
  BufferFrequency frequency;
  BufferUsage usage;
  BufferMetadata *outMeta;
  QueueOwnership initialOwnership;
};
Buffer allocateBuffer(LContext context, BufferSpecification &spec);
void uploadBufferData(LContext context, Buffer buffer, const void *data,
                      size_t offset, size_t size);
struct BufferCopySpecification {
  Buffer source;
  BufferMetadata sourceMeta;
  BufferMetadata *outSourceMeta;
  Buffer destination;
  BufferMetadata destinationMeta;
  BufferMetadata *outDestinationMeta;
  uint32_t sourceOffset;
  uint32_t destinationOffset;
  uint32_t size;
  QueueOwnership finalOwnership;
};
void copyBufferData(LContext context, const BufferCopySpecification &spec);
void destroyBuffer(LContext context, Buffer buffer);

//
// IMAGE
//
struct ImageMetadata {
  ImageLayout layout;
  QueueOwnership ownership;
  PipelineStage stage;
  ResourceAccess access;
};
struct ImageSpecification {
  uint32_t width;
  uint32_t height;
  PixelFormat pixelFormat;
  ImageUsage usage;
  ImageTiling tiling;
  ImageLayout initialLayout;
  ImageMetadata *outMeta;
};
struct ImageCopySpecification {
  Image source;
  ImageMetadata sourceMeta;
  ImageMetadata *outSourceMeta;
  Image destination;
  ImageMetadata destinationMeta;
  ImageMetadata *outDestinationMeta;
  uint32_t srcX;
  uint32_t srcY;
  uint32_t dstX;
  uint32_t dstY;
  uint32_t width;
  uint32_t height;
  ImageLayout finalLayout;
  QueueOwnership finalOwnership;
};
struct ImageBlitSpecification {
  Image source;
  ImageMetadata sourceMeta;
  ImageMetadata *outSourceMeta;
  Image destination;
  ImageMetadata destinationMeta;
  ImageMetadata *outDestinationMeta;
  SampleFilter filter;
  ImageLayout finalLayout;
  QueueOwnership finalOwnership;
};
struct ImageDataCopySpecification {
  Image image;
  ImageMetadata imageMeta;
  ImageMetadata *outImageMeta;
  Buffer stagingBuffer;
  BufferMetadata stagingBufferMeta;
  BufferMetadata *outStaingBufferMeta;
  size_t bufferOffset;
  uint32_t width;
  uint32_t height;
  QueueOwnership finalOwnership;
};
Image createImage(LContext context, const ImageSpecification &spec);
void copyImageData(LContext context, const ImageDataCopySpecification &spec);
void copyImage(LContext context, const ImageCopySpecification &spec);
void blitImage(LContext context, const ImageBlitSpecification &spec);
void destroyImage(LContext context, Image image);
void graphicsAcquireImage(LContext context, Image image, ImageMetadata meta,
                          ImageMetadata *outMeta);

//
// SAMPLER
//
struct SamplerSpecification {
  SampleFilter minFilter;
  SampleFilter magFilter;
  SampleAddressing addressingU;
  SampleAddressing addressingV;
  SampleAddressing addressingW;
};
Sampler createSampler(LContext context, const SamplerSpecification &spec);
void destroySampler(LContext context, Sampler sampler);

//
// SHADER
//
Shader loadSPIRVShader(LContext context, uint8_t bytes[], size_t size);
void destroyShader(LContext context, Shader shader);

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
  float depthMin;
  float depthMax;
  bool flipViewportY;
  bool enableDepthTest;
  bool enableDepthWrite;
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

//
// UNIFORMS
//
struct DescriptorSetLayoutSpecification {
  ResourceType *types;
  ShaderVisibility *visibilities;
  uint32_t typeCount;
};
DescriptorSetLayout
createDescriptorSetLayout(LContext context,
                          const DescriptorSetLayoutSpecification &spec);
void destroyDescriptorSetLayout(LContext context, DescriptorSetLayout layout);

//
// RENDER PASS
//
struct RenderTarget {
  Image image;
  ImageLayout targetLayout;
  RenderTargetType type;
  LoadOp loadOp;
  StoreOp storeOp;
  // Can be unioned with float (depth) + uint32_t (stencil)
  union {
    float clearColor[4];
    float clearDepth;
    uint32_t clearStencil;
  };
};
struct RenderPassSpecification {
  RenderTarget *renderTargets;
  uint32_t renderTargetCount;
  RenderTarget *depthTarget;
  uint32_t width;
  uint32_t height;
};
RenderPass createRenderPass(LContext context,
                            const RenderPassSpecification &spec);
void destroyRenderPass(LContext context, RenderPass renderPass);

//
// SWAPCHAIN
//
struct SwapchainSpecification {
  uint32_t width;
  uint32_t height;
  Swapchain retiredSwapchain;
  bool vSync;
  // While VSync ensures that the maximum framerate is kept at monitor refresh
  // rate, it might be that not every frame is spaced equally - this results in
  // perceived visual stutters. Framerate stabilization ensures frame spacing to
  // eliminate such stutters - set desire framerate here (should be less or
  // equal to monitor refresh rate).
  uint32_t stabilizeFramerate;
};
Swapchain createSwapchain(LContext context, const SwapchainSpecification &spec);
void present(LContext context, Swapchain swapchain, Image image,
             ImageMetadata meta, ImageMetadata *outMeta,
             SampleFilter blitFilter = SampleFilter::LINEAR);
void destroySwapchain(LContext context, Swapchain swapchain);

//
// DESCRIPTORS
//
struct DescriptorUpdateSpecification {
  uint32_t offset;
  ResourceType type;
  Buffer buffer;
  Image image;
  Sampler sampler;
};
struct DescriptorSetUpdateSpecification {
  DescriptorSet descriptorSet;
  DescriptorUpdateSpecification *specs;
  uint32_t specCount;
};
struct DescriptorPoolSpecification {
  uint32_t uniformBufferCount;
  uint32_t combinedSamplerCount;
  uint32_t sampledImageCount;
  uint32_t storageImageCount;
  uint32_t maxSets;
};
DescriptorPool createDescriptorPool(LContext context,
                                    const DescriptorPoolSpecification &spec);
void destroyDescriptorPool(LContext context, DescriptorPool descriptorPool);
DescriptorSet createDescriptorSet(LContext context,
                                  DescriptorPool descriptorPool,
                                  DescriptorSetLayout descriptorSetLayout);
void updateDescriptorSet(LContext context,
                         const DescriptorSetUpdateSpecification &spec);
void destroyDescriptorSet(LContext context, DescriptorSet descriptorSet);

//
// DRAW PROCESSOR
//
struct DrawOperation {
  DrawOperationType type;
  uint32_t target;
  uint32_t count = 1;
};
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
struct PushConstantUpload {
  ShaderVisibility visibility;
  uint8_t bytes[64];
  uint32_t offset;
  uint32_t size;
};
struct VertexBinding {
  Buffer buffer;
  uint32_t binding = UINT32_MAX;
  uint32_t offset = 0;
};
struct IndexBinding {
  Buffer buffer;
  uint32_t offset = 0;
};
struct DrawCall {
  uint32_t elementCount;
  uint32_t instanceCount;
  // TODO: There is room for improvement when it comes to cache locality here
  bool indexed;
  uint32_t firstElement = 0;
  uint32_t firstInstance = 0;
  // This parameter only applies if indexed is true
  uint32_t vertexOffset = 0;
};
struct DispatchCall {
  uint32_t groupCountX;
  uint32_t groupCountY;
  uint32_t groupCountZ;
};
struct BlitCall {
  Image from;
  Image to;
  SampleFilter filter;
  // TODO: Could add size parameters
};
// IMPORTANT: For changes here, see enums.h - DrawOperationType
struct DrawCommand {
  DrawOperation *operations;
  RenderPass *renderPasses;
  Pipeline *pipelines;
  DescriptorSetBinding *descriptorSetBindings;
  PushConstantUpload *pushConstants;
  ImageBarrier *imageBarriers;
  BufferBarrier *bufferBarriers;
  VertexBinding *vertexBindings;
  IndexBinding *indexBindings;
  DrawCall *drawCalls;
  DispatchCall *dispatchCalls;
  BlitCall *blitCalls;
};
DrawProcessor createDrawProcessor(LContext context);
void setDrawProcessorSecondaries(LContext context, DrawProcessor drawProcessor,
                                 uint32_t operationCount,
                                 uint32_t secondaryCount);
void resetDrawProcessor(LContext context, DrawProcessor drawProcessor);
// Records the entire call into the primary command buffer
void processDraw(LContext context, DrawProcessor drawProcessor,
                 const DrawCommand &drawCommand, uint32_t operationCount);
// Records a partial call into the requested secondary
void processDrawSecondary(LContext context, DrawProcessor drawProcessor,
                          const DrawCommand &drawCommand,
                          uint32_t secondaryIndex);
// Submits all recorded secondaries using the primary
void submitSecondaries(LContext context, DrawProcessor drawProcessor);
void destroyDrawProcessor(LContext context, DrawProcessor drawProcessor);
