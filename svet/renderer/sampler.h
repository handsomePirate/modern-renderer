#pragma once
#include "context.h"
#include "enums.h"

namespace svet::renderer {

using Sampler = struct SamplerT *;

struct SamplerSpecification {
  SampleFilter minFilter;
  SampleFilter magFilter;
  SampleAddressing addressingU;
  SampleAddressing addressingV;
  SampleAddressing addressingW;
};
Sampler createSampler(LContext context, const SamplerSpecification &spec);
void destroySampler(LContext context, Sampler sampler);

} // namespace svet::renderer
