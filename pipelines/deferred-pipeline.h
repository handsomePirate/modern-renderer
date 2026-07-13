#pragma once
#include "frame.h"
#include "passes/deferred-base.h"
#include "passes/deferred-shading.h"
#include "passes/overlay-pass.h"
#include "passes/shadow-pass.h"
#include "passes/weighted-oit.h"
#include "svet/renderer/context.h"
#include "svet/renderer/image.h"

struct DeferredPipeline {
  ShadowPass shadowPass;
  DeferredBasePass basePass;
  DeferredShadingPass shadingPass;
  WeightedOITPass weightedOITPass;
  OverlayPass overlayPass;
};

struct DeferredPipelineSpecification {
  uint32_t width;
  uint32_t height;
  uint32_t shadowMapResolution;
  svet::renderer::MemoryPool uniformBufferPool;
  svet::renderer::MemoryPool targetImagePool;
  svet::renderer::PixelFormat colorPixelFormat;
  svet::renderer::PixelFormat positionPixelFormat;
  svet::renderer::PixelFormat depthPixelFormat;
  svet::renderer::PixelFormat shadowMapPixelFormat;
  svet::renderer::DescriptorPool descriptorPool;
};
DeferredPipeline
createDeferredPipeline(svet::renderer::LContext context,
                       const DeferredPipelineSpecification &spec);
void destroyDeferredPipeline(svet::renderer::LContext context,
                             DeferredPipeline &pipeline);
void deferredPipelineDrawFrame(FrameData &frame, DeferredPipeline &pipeline,
                               Scene &scene, svet::renderer::Image &result,
                               svet::renderer::ImageMetadata &resultMeta);
