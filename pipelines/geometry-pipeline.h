#pragma once
#include "passes/geometry-pass.h"
#include "renderer.h"

struct GeometryPipeline {
  GeometryPass pass;
  DrawProcessor drawProcessor;
};

struct GeometryPipelineSpecification {
  uint32_t width;
  uint32_t height;
  PixelFormat colorPixelFormat;
  PixelFormat depthPixelFormat;
  DrawProcessor drawProcessor;
};
GeometryPipeline
createGeometryPipeline(LContext context,
                       const GeometryPipelineSpecification &spec);
void destroyGeometryPipeline(LContext context, GeometryPipeline &pipeline);
void geometryPipelineDrawFrame(LContext context, GeometryPipeline &pipeline,
                               float timeSeconds, DrawCommand &drawCommand,
                               DrawCommandIndexes &indexes, Scene &scene,
                               Image &result, ImageMetadata &resultMeta);
