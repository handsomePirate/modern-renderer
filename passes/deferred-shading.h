#pragma once
#include "renderer.h"
#include "types.h"

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
  Image target;
  Buffer shadingParamsBuffer;

  DescriptorSetLayout gBufferSetLayout;
  PipelineLayout pipelineLayout;
  Pipeline pipeline;

  DescriptorSet gBufferSet;

  uint32_t width;
  uint32_t height;
  uint32_t shadowMapResolution;
};

struct DeferredShadingPassSpecification {
  Image gBuffer[5];
  Image shadowMap;
  DescriptorPool descriptorPool;
  uint32_t width;
  uint32_t height;
  uint32_t shadowMapResolution;
  PixelFormat colorPixelFormat;
  const char *compFile;
};
DeferredShadingPass
createDeferredShadingPass(LContext context,
                          const DeferredShadingPassSpecification &spec);
void destroyDeferredShadingPass(LContext context, DeferredShadingPass &pass);
void recordDeferredShadingPass(LContext context,
                               const DeferredShadingPass &pass,
                               const Scene &scene, DrawCommandIndexes &indexes,
                               DrawCommand &drawCommand);
