#pragma once
#include "pipelines/frame.h"
#include "svet/renderer/context.h"
#include "svet/renderer/descriptor.h"
#include "svet/renderer/image.h"
#include "svet/renderer/memory.h"
#include "svet/renderer/pipeline.h"

#include <glm/glm.hpp>

struct Scene;

struct DeferredShadingParams {
  glm::mat4 sunOrto;
  glm::vec3 sunDir;
  float sunIntensity;
  glm::vec3 sunCol;
  float shadowMapRes;
  glm::vec3 camPos;
};

struct DeferredShadingPass {
  svet::renderer::Image target;
  svet::renderer::Buffer shadingParamsBuffer;

  svet::renderer::DescriptorSetLayout gBufferSetLayout;
  svet::renderer::PipelineLayout pipelineLayout;
  svet::renderer::Pipeline pipeline;

  svet::renderer::DescriptorSet gBufferSet;

  uint32_t width;
  uint32_t height;
  uint32_t shadowMapResolution;
};

struct DeferredShadingPassSpecification {
  svet::renderer::Image gBuffer[5];
  svet::renderer::Image shadowMap;
  svet::renderer::MemoryPool uniformBufferPool;
  svet::renderer::MemoryPool targetImagePool;
  svet::renderer::DescriptorPool descriptorPool;
  uint32_t width;
  uint32_t height;
  uint32_t shadowMapResolution;
  svet::renderer::PixelFormat colorPixelFormat;
  const char *compFile;
};
DeferredShadingPass
createDeferredShadingPass(svet::renderer::LContext context,
                          const DeferredShadingPassSpecification &spec);
void destroyDeferredShadingPass(svet::renderer::LContext context,
                                DeferredShadingPass &pass);
void recordDeferredShadingPass(FrameData &frame,
                               const DeferredShadingPass &pass,
                               const Scene &scene);
