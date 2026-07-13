#pragma once
#include "context.h"
#include "enums.h"
#include "image.h"

#include <cstdint>

namespace svet::renderer {

using Swapchain = struct SwapchainT *;

struct SwapchainSpecification {
  uint32_t width;
  uint32_t height;
  Swapchain retiredSwapchain;
  bool vSync;
  // While VSync ensures that the maximum framerate is kept at monitor refresh
  // rate, it might be that not every frame is spaced equally - this results in
  // perceived visual stutters. Framerate stabilization ensures frame spacing to
  // eliminate such stutters - set desire framerate here (should be less or
  // equal to monitor refresh rate).
  uint32_t stabilizeFramerate;
};
Swapchain createSwapchain(LContext context, const SwapchainSpecification &spec);
void present(LContext context, Swapchain swapchain, Image image,
             ImageMetadata meta, ImageMetadata *outMeta,
             SampleFilter blitFilter = SampleFilter::LINEAR);
void destroySwapchain(LContext context, Swapchain swapchain);

} // namespace svet::renderer
