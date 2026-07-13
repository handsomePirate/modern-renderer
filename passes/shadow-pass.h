#pragma once
#include "pipelines/frame.h"
#include "svet/renderer/context.h"
#include "svet/renderer/image.h"
#include "svet/renderer/pipeline.h"
#include "svet/renderer/render-pass.h"

struct Scene;

// TODO: Perspective shadow maps (PSMs) could be used to improve shadow map
// resolution near the camera
// https://inria.hal.science/inria-00606726/file/PerspectiveShadowMaps.pdf
// Fixes here:
// https://developer.nvidia.com/gpugems/gpugems/part-ii-lighting-and-shadows/chapter-14-perspective-shadow-maps-care-and-feeding
struct ShadowPass {
  svet::renderer::Image target;

  svet::renderer::PipelineLayout pipelineLayout;
  svet::renderer::Pipeline pipeline;

  svet::renderer::RenderPass renderPass;
};

struct ShadowPassSpecification {
  uint32_t width;
  uint32_t height;
  svet::renderer::MemoryPool targetImagePool;
  svet::renderer::PixelFormat depthPixelFormat;
  const char *vertFile;
  const char *fragFile;
};
ShadowPass createShadowPass(svet::renderer::LContext context,
                            const ShadowPassSpecification &spec);
void destroyShadowPass(svet::renderer::LContext context, ShadowPass &pass);
void recordShadowPass(FrameData &frame, const ShadowPass &pass,
                      const Scene &scene);
