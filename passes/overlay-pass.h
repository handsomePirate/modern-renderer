#pragma once
#include "pipelines/frame.h"
#include "svet/renderer/context.h"
#include "svet/renderer/image.h"
#include "svet/renderer/pipeline.h"

struct Scene;

struct OverlayPass {
  svet::renderer::Image target;
  svet::renderer::Image background;
  svet::renderer::Image foreground;

  svet::renderer::DescriptorSetLayout setLayout;
  svet::renderer::PipelineLayout pipelineLayout;
  svet::renderer::Pipeline pipeline;

  svet::renderer::DescriptorSet set;

  uint32_t width;
  uint32_t height;
};

struct OverlayPassSpecification {
  svet::renderer::Image background;
  svet::renderer::Image foreground;
  svet::renderer::MemoryPool targetImagePool;
  svet::renderer::DescriptorPool descriptorPool;
  uint32_t width;
  uint32_t height;
  svet::renderer::PixelFormat colorPixelFormat;
  const char *compFile;
};
OverlayPass createOverlayPass(svet::renderer::LContext context,
                              const OverlayPassSpecification &spec);
void destroyOverlayPass(svet::renderer::LContext context, OverlayPass &pass);
void recordOverlayPass(FrameData &frame, const OverlayPass &pass,
                       const Scene &scene);
