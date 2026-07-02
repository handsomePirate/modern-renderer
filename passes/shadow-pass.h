#pragma once
#include "renderer.h"
#include "types.h"

struct Scene;

// TODO: Perspective shadow maps (PSMs) could be used to improve shadow map
// resolution near the camera
// https://inria.hal.science/inria-00606726/file/PerspectiveShadowMaps.pdf
// Fixes here:
// https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-14-perspective-shadow-maps-care-and-feeding
struct ShadowPass {
  Image target;

  PipelineLayout pipelineLayout;
  Pipeline pipeline;

  RenderPass renderPass;
};

struct ShadowPassSpecification {
  uint32_t width;
  uint32_t height;
  PixelFormat depthPixelFormat;
  const char *vertFile;
  const char *fragFile;
};
ShadowPass createShadowPass(LContext context,
                            const ShadowPassSpecification &spec);
void destroyShadowPass(LContext context, ShadowPass &pass);
void recordShadowPass(LContext context, const ShadowPass &pass,
                      const Scene &scene, DrawCommandIndexes &indexes,
                      DrawCommand &drawCommand);
