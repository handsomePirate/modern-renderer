#pragma once
#include "pipelines/frame.h"
#include "svet/renderer/context.h"
#include "svet/renderer/image.h"
#include "svet/renderer/pipeline.h"
#include "svet/renderer/render-pass.h"

struct Scene;

struct GeometryPass {
  svet::renderer::Image colorTarget;
  svet::renderer::Image depthTarget;
  bool shouldDestroyColorTarget;
  bool shouldDestroyDepthTarget;

  svet::renderer::PipelineLayout pipelineLayout;
  svet::renderer::Pipeline pipeline;

  svet::renderer::RenderPass renderPass;
};

struct GeometryPassSpecification {
  svet::renderer::Image colorTarget;
  svet::renderer::Image depthTarget;
  svet::renderer::MemoryPool targetImagePool;
  uint32_t width;
  uint32_t height;
  svet::renderer::LoadOp colorTargetLoadOp;
  svet::renderer::LoadOp depthTargetLoadOp;
  svet::renderer::PixelFormat colorPixelFormat;
  svet::renderer::PixelFormat depthPixelFormat;
  const char *vertFile;
  const char *fragFile;
};

GeometryPass createGeometryPass(svet::renderer::LContext context,
                                const GeometryPassSpecification &spec);
void destroyGeometryPass(svet::renderer::LContext context, GeometryPass &pass);
void recordGeometryPass(FrameData &frame, const GeometryPass &pass,
                        const Scene &scene);
