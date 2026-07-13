#pragma once
#include "pipelines/frame.h"
#include "svet/renderer/context.h"
#include "svet/renderer/image.h"
#include "svet/renderer/memory.h"
#include "svet/renderer/pipeline.h"
#include "svet/renderer/render-pass.h"

struct Scene;

struct WeightedOITPass {
  svet::renderer::Image accumulator;
  svet::renderer::Image reveal;
  svet::renderer::Image resolve;
  uint32_t width;
  uint32_t height;
  uint32_t shadowMapResolution;

  svet::renderer::Image inheritedDepth;

  svet::renderer::DescriptorSetLayout shadingLayout;
  svet::renderer::PipelineLayout graphicsPipelineLayout;
  svet::renderer::Pipeline graphicsPipeline;
  svet::renderer::DescriptorSet shadingSet;

  svet::renderer::RenderPass renderPass;

  svet::renderer::DescriptorSetLayout computeLayout;
  svet::renderer::PipelineLayout computePipelineLayout;
  svet::renderer::Pipeline computePipeline;
  svet::renderer::DescriptorSet computeSet;

  svet::renderer::Buffer shadingParamsBuffer;
};

struct WeightedOITSpecification {
  uint32_t width;
  uint32_t height;
  uint32_t shadowMapResolution;
  svet::renderer::MemoryPool uniformBufferPool;
  svet::renderer::MemoryPool targetImagePool;
  svet::renderer::DescriptorPool descriptorPool;
  svet::renderer::PixelFormat accumulatorFormat;
  svet::renderer::PixelFormat revealFormat;
  svet::renderer::PixelFormat colorPixelFormat;
  svet::renderer::PixelFormat depthPixelFormat;
  const char *vertFile;
  const char *fragFile;
  const char *compFile;
  svet::renderer::Image inheritedDepth;
  svet::renderer::Image shadowMap;
};
WeightedOITPass createWeightedOITPass(svet::renderer::LContext context,
                                      const WeightedOITSpecification &spec);
void destroyWeightedOITPass(svet::renderer::LContext context,
                            WeightedOITPass &pass);
void recordWeightedOITPassDrawScene(FrameData &frame,
                                    const WeightedOITPass &pass,
                                    const Scene &scene);
