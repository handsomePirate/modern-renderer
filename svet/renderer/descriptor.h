#pragma once
#include "buffer.h"
#include "context.h"
#include "enums.h"
#include "image.h"
#include "sampler.h"

#include <cstdint>

namespace svet::renderer {

using DescriptorSetLayout = struct DescriptorSetLayoutT *;
using DescriptorPool = struct DescriptorPoolT *;
using DescriptorSet = struct DescriptorSetT *;

struct DescriptorSetLayoutSpecification {
  ResourceType *types;
  ShaderVisibility *visibilities;
  uint32_t typeCount;
};
DescriptorSetLayout
createDescriptorSetLayout(LContext context,
                          const DescriptorSetLayoutSpecification &spec);
void destroyDescriptorSetLayout(LContext context, DescriptorSetLayout layout);

struct DescriptorUpdateSpecification {
  uint32_t offset;
  ResourceType type;
  Buffer buffer;
  Image image;
  Sampler sampler;
};
struct DescriptorSetUpdateSpecification {
  DescriptorSet descriptorSet;
  DescriptorUpdateSpecification *specs;
  uint32_t specCount;
};
struct DescriptorPoolSpecification {
  uint32_t uniformBufferCount;
  uint32_t combinedSamplerCount;
  uint32_t sampledImageCount;
  uint32_t storageImageCount;
  uint32_t maxSets;
};
DescriptorPool createDescriptorPool(LContext context,
                                    const DescriptorPoolSpecification &spec);
void destroyDescriptorPool(LContext context, DescriptorPool descriptorPool);
DescriptorSet createDescriptorSet(LContext context,
                                  DescriptorPool descriptorPool,
                                  DescriptorSetLayout descriptorSetLayout);
void updateDescriptorSet(LContext context,
                         const DescriptorSetUpdateSpecification &spec);
void destroyDescriptorSet(LContext context, DescriptorSet descriptorSet);

} // namespace svet::renderer
