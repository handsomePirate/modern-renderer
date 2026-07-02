#pragma once
#include "passes/deferred-base.h"
#include "passes/deferred-shading.h"
#include "passes/overlay-pass.h"
#include "passes/shadow-pass.h"
#include "passes/weighted-oit.h"
#include "renderer.h"

struct DeferredPipeline {
  ShadowPass shadowPass;
  DeferredBasePass basePass;
  DeferredShadingPass shadingPass;
  WeightedOITPass weightedOITPass;
  OverlayPass overlayPass;
  DrawProcessor drawProcessor;
};

struct DeferredPipelineSpecification {
  uint32_t width;
  uint32_t height;
  uint32_t shadowMapResolution;
  PixelFormat colorPixelFormat;
  PixelFormat positionPixelFormat;
  PixelFormat depthPixelFormat;
  PixelFormat shadowMapPixelFormat;
  DescriptorPool descriptorPool;
  DrawProcessor drawProcessor;
};
DeferredPipeline
createDeferredPipeline(LContext context,
                       const DeferredPipelineSpecification &spec);
void destroyDeferredPipeline(LContext context, DeferredPipeline &pipeline);
void deferredPipelineDrawFrame(LContext context, DeferredPipeline &pipeline,
                               float timeSeconds, DrawCommand &drawCommand,
                               DrawCommandIndexes &indexes, Scene &scene,
                               Image &result, ImageMetadata &resultMeta);
