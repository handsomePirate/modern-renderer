#pragma once
#include "svet/renderer/context.h"
#include "svet/renderer/draw-processor.h"

#include <pool-allocator/linear_allocator.hpp>

struct FrameData {
  svet::renderer::LContext context;
  svet::renderer::DrawProcessor drawProcessor;
  allocation::linear_allocator_heap<2048> memory;
  float timeSeconds;
};
