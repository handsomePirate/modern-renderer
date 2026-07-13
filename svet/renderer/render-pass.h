#pragma once
#include "context.h"
#include "enums.h"
#include "image.h"

#include <cstdint>

namespace svet::renderer {

using RenderPass = struct RenderPassT *;

struct RenderTarget {
  Image image;
  ImageLayout targetLayout;
  RenderTargetType type;
  LoadOp loadOp;
  StoreOp storeOp;
  // Can be unioned with float (depth) + uint32_t (stencil)
  union {
    float clearColor[4];
    float clearDepth;
    uint32_t clearStencil;
  };
};
struct RenderPassSpecification {
  RenderTarget *renderTargets;
  uint32_t renderTargetCount;
  RenderTarget *depthTarget;
  uint32_t width;
  uint32_t height;
};
RenderPass createRenderPass(LContext context,
                            const RenderPassSpecification &spec);
void destroyRenderPass(LContext context, RenderPass renderPass);

} // namespace svet::renderer
