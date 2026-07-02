#pragma once
#include "renderer.h"
#include "types.h"

struct Scene;

struct DeferredBasePass {
  Image gBuffer[5];

  PipelineLayout pipelineLayout;
  Pipeline pipeline;

  RenderPass renderPass;
};

struct DeferredBasePassSpecification {
  uint32_t width;
  uint32_t height;
  PixelFormat colorPixelFormat;
  PixelFormat positionPixelFormat;
  PixelFormat depthPixelFormat;
  const char *vertFile;
  const char *fragFile;
};
DeferredBasePass
createDeferredBasePass(LContext context,
                       const DeferredBasePassSpecification &spec);
void destroyDeferredBasePass(LContext context, DeferredBasePass &pass);
void recordDeferredBasePass(LContext context, const DeferredBasePass &pass,
                            const Scene &scene, DrawCommandIndexes &indexes,
                            DrawCommand &drawCommand);
