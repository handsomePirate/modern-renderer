#pragma once
#include "renderer.h"
#include "types.h"

struct Scene;

struct WeightedOITPass {
  Image accumulator;
  Image reveal;
  Image resolve;
  uint32_t width;
  uint32_t height;
  uint32_t shadowMapResolution;

  Image inheritedDepth;

  DescriptorSetLayout shadingLayout;
  PipelineLayout graphicsPipelineLayout;
  Pipeline graphicsPipeline;
  DescriptorSet shadingSet;

  RenderPass renderPass;

  DescriptorSetLayout computeLayout;
  PipelineLayout computePipelineLayout;
  Pipeline computePipeline;
  DescriptorSet computeSet;

  Buffer shadingParamsBuffer;
};

struct WeightedOITSpecification {
  uint32_t width;
  uint32_t height;
  uint32_t shadowMapResolution;
  DescriptorPool descriptorPool;
  PixelFormat accumulatorFormat;
  PixelFormat revealFormat;
  PixelFormat colorPixelFormat;
  PixelFormat depthPixelFormat;
  const char *vertFile;
  const char *fragFile;
  const char *compFile;
  Image inheritedDepth;
  Image shadowMap;
};
WeightedOITPass createWeightedOITPass(LContext context,
                                      const WeightedOITSpecification &spec);
void destroyWeightedOITPass(LContext context, WeightedOITPass &pass);
void recordWeightedOITPassDrawScene(LContext context,
                                    const WeightedOITPass &pass,
                                    const Scene &scene,
                                    DrawCommandIndexes &indexes,
                                    DrawCommand &drawCommand);
