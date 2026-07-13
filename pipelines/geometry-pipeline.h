#pragma once
#include "frame.h"
#include "passes/geometry-pass.h"
#include "svet/renderer/context.h"
#include "svet/renderer/image.h"

struct GeometryPipeline {
  GeometryPass pass;
};

struct GeometryPipelineSpecification {
  uint32_t width;
  uint32_t height;
  svet::renderer::MemoryPool targetImagePool;
  svet::renderer::PixelFormat colorPixelFormat;
  svet::renderer::PixelFormat depthPixelFormat;
};
GeometryPipeline
createGeometryPipeline(svet::renderer::LContext context,
                       const GeometryPipelineSpecification &spec);
void destroyGeometryPipeline(svet::renderer::LContext context,
                             GeometryPipeline &pipeline);
void geometryPipelineDrawFrame(FrameData &frame, GeometryPipeline &pipeline,
                               Scene &scene, svet::renderer::Image &result,
                               svet::renderer::ImageMetadata &resultMeta);
