#include "descriptor.h"
#include "helpers.h"
#include "types.h"

#include <vulkan/vulkan.h>

#include <cstring>

namespace svet::renderer {

DescriptorSetLayout
createDescriptorSetLayout(LContext context,
                          const DescriptorSetLayoutSpecification &spec) {
  auto uniformSet = new DescriptorSetLayoutT;
  uniformSet->types = new ResourceType[spec.typeCount];
  uniformSet->typeCount = spec.typeCount;
  std::memcpy(uniformSet->types, spec.types,
              spec.typeCount * sizeof(ResourceType));
  VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
  setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  auto bindings = new VkDescriptorSetLayoutBinding[spec.typeCount]{};
  for (int i = 0; i < spec.typeCount; ++i) {
    bindings[i].binding = i;
    bindings[i].descriptorType = getDescriptorType(spec.types[i]);
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = getShaderStageFlags(spec.visibilities[i]);
  }
  setLayoutInfo.pBindings = bindings;
  setLayoutInfo.bindingCount = spec.typeCount;
  vkCreateDescriptorSetLayout(context->device, &setLayoutInfo, nullptr,
                              &uniformSet->descriptorSetLayout);
  delete[] bindings;
  return uniformSet;
}

void destroyDescriptorSetLayout(LContext context, DescriptorSetLayout layout) {
  vkDestroyDescriptorSetLayout(context->device, layout->descriptorSetLayout,
                               nullptr);
  delete[] layout->types;
  delete layout;
}

DescriptorPool createDescriptorPool(LContext context,
                                    const DescriptorPoolSpecification &spec) {
  auto descriptorPool = new DescriptorPoolT;

  VkDescriptorPoolSize poolSizes[4];
  uint32_t poolCount = 0;

  if (spec.uniformBufferCount > 0) {
    poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[poolCount].descriptorCount = spec.uniformBufferCount;
    ++poolCount;
  }

  if (spec.combinedSamplerCount > 0) {
    poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[poolCount].descriptorCount = spec.combinedSamplerCount;
    ++poolCount;
  }

  if (spec.sampledImageCount > 0) {
    poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[poolCount].descriptorCount = spec.sampledImageCount;
    ++poolCount;
  }

  if (spec.storageImageCount > 0) {
    poolSizes[poolCount].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[poolCount].descriptorCount = spec.storageImageCount;
    ++poolCount;
  }

  if (poolCount == 0 || spec.maxSets == 0) {
    return nullptr;
  }

  VkDescriptorPoolCreateInfo descriptorPoolInfo{};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.pPoolSizes = poolSizes;
  descriptorPoolInfo.poolSizeCount = poolCount;
  descriptorPoolInfo.maxSets = spec.maxSets;

  vkCreateDescriptorPool(context->device, &descriptorPoolInfo, nullptr,
                         &descriptorPool->descriptorPool);

  return descriptorPool;
}

void destroyDescriptorPool(LContext context, DescriptorPool descriptorPool) {
  vkDestroyDescriptorPool(context->device, descriptorPool->descriptorPool,
                          nullptr);
  delete descriptorPool;
}

DescriptorSet createDescriptorSet(LContext context,
                                  DescriptorPool descriptorPool,
                                  DescriptorSetLayout descriptorSetLayout) {
  auto descriptorSet = new DescriptorSetT;
  descriptorSet->pool = descriptorPool;

  VkDescriptorSetAllocateInfo allocateInfo{};
  allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocateInfo.descriptorPool = descriptorPool->descriptorPool;
  allocateInfo.pSetLayouts = &descriptorSetLayout->descriptorSetLayout;
  allocateInfo.descriptorSetCount = 1;
  auto res = vkAllocateDescriptorSets(context->device, &allocateInfo,
                                      &descriptorSet->descriptorSet);
  if (res != VK_SUCCESS) {
    std::println("[Vulkan] Failed to allocate descriptor set: {}", (int)res);
  }

  return descriptorSet;
}

void updateDescriptorSet(LContext context,
                         const DescriptorSetUpdateSpecification &spec) {
  if (spec.specs) {
    uint32_t specsRemaining = spec.specCount;
    const uint32_t maxWrites = 64;
    const uint32_t loopCount = (spec.specCount + maxWrites - 1) / maxWrites;
    const uint32_t finalRemainder = spec.specCount % maxWrites;
    for (uint32_t i = 0; i < loopCount; ++i) {
      VkWriteDescriptorSet writes[maxWrites];
      VkDescriptorBufferInfo bufferInfo[maxWrites];
      VkDescriptorImageInfo imageInfo[maxWrites];
      uint32_t baseIndex = i * maxWrites;
      uint32_t writeCount = (i == loopCount - 1 && finalRemainder != 0)
                                ? (spec.specCount % maxWrites)
                                : maxWrites;

      for (int j = 0; j < writeCount; ++j) {
        int index = baseIndex + j;
        auto type = spec.specs[index].type;
        writes[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[j].descriptorType = getDescriptorType(type);
        // TODO: Bunch these up if possible
        writes[j].descriptorCount = 1;
        writes[j].dstBinding = index;
        writes[j].dstSet = spec.descriptorSet->descriptorSet;
        writes[j].dstArrayElement = 0;
        writes[j].pBufferInfo = nullptr;
        writes[j].pImageInfo = nullptr;
        writes[j].pTexelBufferView = nullptr;
        writes[j].pNext = nullptr;
        if (type == ResourceType::UNIFORM_BUFFER) {
          bufferInfo[j].buffer = spec.specs[index].buffer->buffer;
          bufferInfo[j].offset = spec.specs[index].offset;
          bufferInfo[j].range = spec.specs[index].buffer->size;
          writes[j].pBufferInfo = &bufferInfo[j];
        } else if (type == ResourceType::COMBINED_SAMPLER) {
          imageInfo[j].imageView = spec.specs[index].image->imageView;
          imageInfo[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          imageInfo[j].sampler = spec.specs[index].sampler->sampler;
          writes[j].pImageInfo = &imageInfo[j];
        } else if (type == ResourceType::SAMPLED_IMAGE) {
          imageInfo[j].imageView = spec.specs[index].image->imageView;
          imageInfo[j].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
          imageInfo[j].sampler = VK_NULL_HANDLE;
          writes[j].pImageInfo = &imageInfo[j];
        } else if (type == ResourceType::STORAGE_IMAGE) {
          imageInfo[j].imageView = spec.specs[index].image->imageView;
          imageInfo[j].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
          imageInfo[j].sampler = VK_NULL_HANDLE;
          writes[j].pImageInfo = &imageInfo[j];
        } else if (type == ResourceType::SAMPLER) {
          imageInfo[j].imageView = VK_NULL_HANDLE;
          imageInfo[j].imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
          imageInfo[j].sampler = spec.specs[index].sampler->sampler;
          writes[j].pImageInfo = &imageInfo[j];
        } else {
          std::println(
              "[Vulkan] Unsupported type for descriptor set update: {}",
              (int)type);
        }
      }

      vkUpdateDescriptorSets(context->device, writeCount, writes, 0, nullptr);
    }
  }
}

void destroyDescriptorSet(LContext context, DescriptorSet descriptorSet) {
  delete descriptorSet;
}

} // namespace svet::renderer
