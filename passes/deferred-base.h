#pragma once
#include "pipelines/frame.h"
#include "svet/renderer/context.h"
#include "svet/renderer/image.h"
#include "svet/renderer/pipeline.h"
#include "svet/renderer/render-pass.h"

struct Scene;

struct DeferredBasePass {
  svet::renderer::Image gBuffer[5];

  svet::renderer::PipelineLayout pipelineLayout;
  svet::renderer::Pipeline pipeline;

  svet::renderer::RenderPass renderPass;
};

struct DeferredBasePassSpecification {
  uint32_t width;
  uint32_t height;
  svet::renderer::MemoryPool targetImagePool;
  svet::renderer::PixelFormat colorPixelFormat;
  svet::renderer::PixelFormat positionPixelFormat;
  svet::renderer::PixelFormat depthPixelFormat;
  const char *vertFile;
  const char *fragFile;
};
DeferredBasePass
createDeferredBasePass(svet::renderer::LContext context,
                       const DeferredBasePassSpecification &spec);
void destroyDeferredBasePass(svet::renderer::LContext context,
                             DeferredBasePass &pass);
void recordDeferredBasePass(FrameData &frame, const DeferredBasePass &pass,
                            const Scene &scene);
