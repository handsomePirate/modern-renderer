#include "pipeline.h"
#include "helpers.h"
#include "types.h"

#include <vulkan/vulkan.h>

namespace svet::renderer {

namespace {

VkVertexInputRate getVkInputRate(VertexInputRate rate) {
  return (rate == VertexInputRate::VERTEX) ? VK_VERTEX_INPUT_RATE_VERTEX
                                           : VK_VERTEX_INPUT_RATE_INSTANCE;
}

VkPrimitiveTopology getPrimitiveTopology(DrawMode drawMode) {
  switch (drawMode) {
  case DrawMode::TRIANGLES:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case DrawMode::TRIANGLE_STRIP:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  }

  std::println("Unsupported draw mode, defaulting to TRIANGLES");
  return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

VkCullModeFlags getCullMode(FaceCulling culling) {
  switch (culling) {
  case FaceCulling::NONE:
    return VK_CULL_MODE_NONE;
  case FaceCulling::BACK:
    return VK_CULL_MODE_BACK_BIT;
  case FaceCulling::FRONT:
    return VK_CULL_MODE_FRONT_BIT;
  }

  std::println("Unsupported face culling, defaulting to NONE");
  return VK_CULL_MODE_NONE;
}

VkBlendOp getBlendOp(BlendOp op) {
  switch (op) {
  case BlendOp::ADD:
    return VK_BLEND_OP_ADD;
  case BlendOp::SUBTRACT:
    return VK_BLEND_OP_SUBTRACT;
  case BlendOp::MIN:
    return VK_BLEND_OP_MIN;
  case BlendOp::MAX:
    return VK_BLEND_OP_MAX;
  }

  std::println("Unsupported blend op, defaulting to ADD");
  return VK_BLEND_OP_ADD;
}

VkBlendFactor getBlendFactor(BlendFactor factor) {
  return (VkBlendFactor)factor;
}

VkColorComponentFlags getColorComponents(ColorComponent components) {
  return (VkColorComponentFlags)components;
}

} // namespace

PipelineLayout createPipelineLayout(LContext context, DescriptorSetLayout *sets,
                                    uint32_t setCount,
                                    PushConstant *pushConstants,
                                    uint32_t constantCount) {
  if (setCount > 4) {
    std::println(
        "[Vulkan] Cannot bind more than 4 descriptor sets at one time");
    return nullptr;
  }
  if (constantCount > 128) {
    std::println("[Vulkan] Cannot request more than 128 push constant ranges "
                 "at one time");
    return nullptr;
  }
  auto pipelineLayout = new PipelineLayoutT;

  VkDescriptorSetLayout layouts[4];
  for (int i = 0; i < setCount; ++i) {
    layouts[i] = sets[i]->descriptorSetLayout;
  }

  // TODO: Base push constants on available push constant memory
  VkPushConstantRange ranges[128];
  for (int i = 0; i < constantCount; ++i) {
    ranges[i].stageFlags = getShaderStageFlags(pushConstants[i].visibility);
    ranges[i].offset = pushConstants[i].offset;
    ranges[i].size = pushConstants[i].size;
  }

  VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
  pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.pSetLayouts = setCount > 0 ? layouts : nullptr;
  pipelineLayoutInfo.setLayoutCount = setCount;
  pipelineLayoutInfo.pPushConstantRanges = constantCount > 0 ? ranges : nullptr;
  pipelineLayoutInfo.pushConstantRangeCount = constantCount;

  vkCreatePipelineLayout(context->device, &pipelineLayoutInfo, nullptr,
                         &pipelineLayout->layout);

  return pipelineLayout;
}

void destroyPipelineLayout(LContext context, PipelineLayout pipelineLayout) {
  vkDestroyPipelineLayout(context->device, pipelineLayout->layout, nullptr);
  delete pipelineLayout;
}

Pipeline createGraphicsPipeline(LContext context,
                                const GraphicsPipelineSpecification &spec) {
  auto pipeline = new PipelineT;
  VkDevice device = context->device;

  pipeline->pipeline = VK_NULL_HANDLE;
  pipeline->layout = spec.pipelineLayout->layout;
  pipeline->isGraphics = true;

  //
  // SHADER STAGES
  //
  VkPipelineShaderStageCreateInfo shaderStages[2]{};

  shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  shaderStages[0].module = spec.vertexShader->module;
  shaderStages[0].pName = "main";

  shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  shaderStages[1].module = spec.fragmentShader->module;
  shaderStages[1].pName = "main";

  //
  // VERTEX INPUT STATE
  //
  VkVertexInputAttributeDescription *attributeDescs = nullptr;
  if (spec.attributeCount > 0) {
    attributeDescs = new VkVertexInputAttributeDescription[spec.attributeCount];

    for (uint32_t i = 0; i < spec.attributeCount; i++) {
      const VertexAttributeDescription &attr = spec.attributes[i];
      attributeDescs[i].location = attr.location;
      attributeDescs[i].binding = attr.binding;
      attributeDescs[i].format = getVkVertexFormat(attr.format);
      attributeDescs[i].offset = attr.offset;
    }
  }

  VkVertexInputBindingDescription *bindingDescs = nullptr;
  if (spec.vertexBindingCount > 0) {
    bindingDescs = new VkVertexInputBindingDescription[spec.vertexBindingCount];

    for (uint32_t i = 0; i < spec.vertexBindingCount; i++) {
      const VertexBufferBinding &binding = spec.vertexBindings[i];
      bindingDescs[i].binding = binding.bindingIndex;
      bindingDescs[i].stride = binding.stride;
      bindingDescs[i].inputRate = getVkInputRate(binding.inputRate);
    }
  }

  VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
  vertexInputInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexAttributeDescriptionCount = spec.attributeCount;
  vertexInputInfo.pVertexAttributeDescriptions = attributeDescs;
  vertexInputInfo.vertexBindingDescriptionCount = spec.vertexBindingCount;
  vertexInputInfo.pVertexBindingDescriptions = bindingDescs;

  //
  // INPUT ASSEMBLY
  //
  VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
  inputAssemblyInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssemblyInfo.topology = getPrimitiveTopology(spec.drawMode);
  inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

  //
  // VIEWPORT & SCISSOR
  //
  VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
  dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicStateInfo.dynamicStateCount = 0;
  dynamicStateInfo.pDynamicStates = nullptr;

  VkViewport viewport{};
  viewport.x = 0;
  viewport.width = spec.width;
  if (spec.flipViewportY) {
    viewport.y = spec.height;
    viewport.height = -(float)spec.height;
  } else {
    viewport.y = 0;
    viewport.height = spec.height;
  }
  viewport.minDepth = spec.depthMin;
  viewport.maxDepth = spec.depthMax;

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {spec.width, spec.height};

  VkPipelineViewportStateCreateInfo viewportStateInfo{};
  viewportStateInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportStateInfo.viewportCount = 1;
  viewportStateInfo.scissorCount = 1;
  viewportStateInfo.pViewports = &viewport;
  viewportStateInfo.pScissors = &scissor;

  //
  // RASTERIZATION
  //
  VkPipelineRasterizationStateCreateInfo rasterizationInfo{};
  rasterizationInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizationInfo.depthClampEnable = VK_FALSE;
  rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
  rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizationInfo.cullMode = getCullMode(spec.faceCulling);
  rasterizationInfo.frontFace = getFrontFace(spec.frontFaceWind);
  rasterizationInfo.depthBiasEnable = VK_FALSE;
  rasterizationInfo.lineWidth = 1.0f;

  //
  // MULTISAMPLE
  //
  VkPipelineMultisampleStateCreateInfo multisampleInfo{};
  multisampleInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampleInfo.sampleShadingEnable = VK_FALSE;
  multisampleInfo.rasterizationSamples =
      (VkSampleCountFlagBits)spec.multiSampleCount;
  multisampleInfo.alphaToCoverageEnable =
      spec.enableAlphaToCoverage ? VK_TRUE : VK_FALSE;

  //
  // DEPTH & STENCIL
  //
  VkPipelineDepthStencilStateCreateInfo depthStencilInfo{};
  depthStencilInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencilInfo.depthTestEnable = spec.enableDepthTest ? VK_TRUE : VK_FALSE;
  depthStencilInfo.depthWriteEnable =
      spec.enableDepthWrite ? VK_TRUE : VK_FALSE;
  depthStencilInfo.depthCompareOp = VK_COMPARE_OP_GREATER;
  depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
  depthStencilInfo.stencilTestEnable = VK_FALSE;
  depthStencilInfo.minDepthBounds = 1.0f;
  depthStencilInfo.maxDepthBounds = 0.0f;

  //
  // COLOR BLENDING
  //
  VkPipelineColorBlendAttachmentState *colorBlendAttachments = nullptr;
  if (spec.renderTargetCount > 0) {
    colorBlendAttachments =
        new VkPipelineColorBlendAttachmentState[spec.renderTargetCount]{};
    for (int i = 0; i < spec.renderTargetCount; ++i) {
      colorBlendAttachments[i].colorWriteMask =
          getColorComponents(spec.renderTargetBlends[i].colorWriteMask);

      if (spec.renderTargetBlends[i].allow) {
        colorBlendAttachments[i].blendEnable = VK_TRUE;
        colorBlendAttachments[i].colorBlendOp =
            getBlendOp(spec.renderTargetBlends[i].colorOp);
        colorBlendAttachments[i].srcColorBlendFactor =
            getBlendFactor(spec.renderTargetBlends[i].srcColorFactor);
        colorBlendAttachments[i].dstColorBlendFactor =
            getBlendFactor(spec.renderTargetBlends[i].dstColorFactor);
        colorBlendAttachments[i].alphaBlendOp =
            getBlendOp(spec.renderTargetBlends[i].alphaOp);
        colorBlendAttachments[i].srcAlphaBlendFactor =
            getBlendFactor(spec.renderTargetBlends[i].srcAlphaFactor);
        colorBlendAttachments[i].dstAlphaBlendFactor =
            getBlendFactor(spec.renderTargetBlends[i].dstAlphaFactor);
      } else {
        colorBlendAttachments[i].blendEnable = VK_FALSE;
      }
    }
  }

  VkPipelineColorBlendStateCreateInfo colorBlendInfo{};
  colorBlendInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlendInfo.logicOpEnable = VK_FALSE;
  colorBlendInfo.pAttachments = colorBlendAttachments;
  colorBlendInfo.attachmentCount = spec.renderTargetCount;

  //
  // PIPELINE
  //
  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
  pipelineInfo.pViewportState = &viewportStateInfo;
  pipelineInfo.pRasterizationState = &rasterizationInfo;
  pipelineInfo.pMultisampleState = &multisampleInfo;
  pipelineInfo.pDepthStencilState = &depthStencilInfo;
  pipelineInfo.pColorBlendState = &colorBlendInfo;
  pipelineInfo.pDynamicState = &dynamicStateInfo;
  pipelineInfo.layout = pipeline->layout;
  pipelineInfo.renderPass = VK_NULL_HANDLE;
  pipelineInfo.subpass = 0;

  VkFormat *formats = nullptr;
  if (spec.renderTargetCount > 0) {
    formats = new VkFormat[spec.renderTargetCount];
    for (int i = 0; i < spec.renderTargetCount; ++i) {
      formats[i] = getVulkanFormat(spec.renderTargetFormats[i]);
    }
  }
  VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
  pipelineRenderingInfo.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  pipelineRenderingInfo.pColorAttachmentFormats = formats;
  pipelineRenderingInfo.colorAttachmentCount = spec.renderTargetCount;
  if (spec.depthPixelFormat) {
    pipelineRenderingInfo.depthAttachmentFormat =
        getVulkanFormat(*spec.depthPixelFormat);
  }
  pipelineInfo.pNext = &pipelineRenderingInfo;

  VkPipeline vkPipeline = VK_NULL_HANDLE;
  VkResult result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline->pipeline);

  if (spec.renderTargetCount > 0) {
    delete[] formats;
    delete[] colorBlendAttachments;
  }
  if (spec.attributeCount > 0)
    delete[] attributeDescs;
  if (spec.vertexBindingCount > 0)
    delete[] bindingDescs;

  return pipeline;
}

Pipeline createComputePipeline(LContext context,
                               const ComputePipelineSpecification &spec) {
  auto pipeline = new PipelineT;
  pipeline->layout = spec.pipelineLayout->layout;
  pipeline->isGraphics = false;

  VkPipelineShaderStageCreateInfo shaderStage{};
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStage.module = spec.computeShader->module;
  shaderStage.pName = "main";

  VkComputePipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineInfo.layout = pipeline->layout;
  pipelineInfo.stage = shaderStage;

  vkCreateComputePipelines(context->device, VK_NULL_HANDLE, 1, &pipelineInfo,
                           nullptr, &pipeline->pipeline);

  return pipeline;
}

void destroyPipeline(LContext context, Pipeline pipeline) {
  vkDestroyPipeline(context->device, pipeline->pipeline, nullptr);
  delete pipeline;
}

} // namespace svet::renderer
