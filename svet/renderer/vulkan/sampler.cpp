#include "sampler.h"
#include "helpers.h"
#include "types.h"

#include <vulkan/vulkan.h>

namespace svet::renderer {

Sampler createSampler(LContext context, const SamplerSpecification &spec) {
  auto sampler = new SamplerT;

  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.minFilter = getVulkanFilter(spec.minFilter);
  samplerInfo.magFilter = getVulkanFilter(spec.magFilter);
  samplerInfo.addressModeU = getAddressMode(spec.addressingU);
  samplerInfo.addressModeV = getAddressMode(spec.addressingV);
  samplerInfo.addressModeW = getAddressMode(spec.addressingW);
  vkCreateSampler(context->device, &samplerInfo, nullptr, &sampler->sampler);

  return sampler;
}

void destroySampler(LContext context, Sampler sampler) {
  vkDestroySampler(context->device, sampler->sampler, nullptr);
  delete sampler;
}

} // namespace svet::renderer
