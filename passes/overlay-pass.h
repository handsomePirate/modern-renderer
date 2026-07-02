#pragma once
#include "renderer.h"
#include "types.h"

struct Scene;

struct OverlayPass {
  Image target;
  Image background;
  Image foreground;

  DescriptorSetLayout setLayout;
  PipelineLayout pipelineLayout;
  Pipeline pipeline;

  DescriptorSet set;

  uint32_t width;
  uint32_t height;
};

struct OverlayPassSpecification {
  Image background;
  Image foreground;
  DescriptorPool descriptorPool;
  uint32_t width;
  uint32_t height;
  PixelFormat colorPixelFormat;
  const char *compFile;
};
OverlayPass createOverlayPass(LContext context,
                              const OverlayPassSpecification &spec);
void destroyOverlayPass(LContext context, OverlayPass &pass);
void recordOverlayPass(LContext context, const OverlayPass &pass,
                       const Scene &scene, DrawCommandIndexes &indexes,
                       DrawCommand &drawCommand);
