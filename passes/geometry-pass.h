#pragma once
#include "renderer.h"
#include "types.h"

struct Scene;

struct GeometryPass {
  Image colorTarget;
  Image depthTarget;
  bool shouldDestroyColorTarget;
  bool shouldDestroyDepthTarget;

  PipelineLayout pipelineLayout;
  Pipeline pipeline;

  RenderPass renderPass;
};

struct GeometryPassSpecification {
  Image colorTarget;
  Image depthTarget;
  uint32_t width;
  uint32_t height;
  LoadOp colorTargetLoadOp;
  LoadOp depthTargetLoadOp;
  PixelFormat colorPixelFormat;
  PixelFormat depthPixelFormat;
  const char *vertFile;
  const char *fragFile;
};

GeometryPass createGeometryPass(LContext context,
                                const GeometryPassSpecification &spec);
void destroyGeometryPass(LContext context, GeometryPass &pass);
void recordGeometryPass(LContext context, const GeometryPass &pass,
                        const Scene &scene, DrawCommandIndexes &indexes,
                        DrawCommand &drawCommand);
