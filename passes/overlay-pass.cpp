#include "overlay-pass.h"
#include "file.h"
#include "scene.h"

OverlayPass createOverlayPass(LContext context,
                              const OverlayPassSpecification &spec) {
  OverlayPass pass;
  pass.width = spec.width;
  pass.height = spec.height;
  pass.background = spec.background;
  pass.foreground = spec.foreground;

  {
    ImageSpecification shadedImageSpec{};
    shadedImageSpec.width = spec.width;
    shadedImageSpec.height = spec.height;
    shadedImageSpec.pixelFormat = spec.colorPixelFormat;
    shadedImageSpec.tiling = ImageTiling::OPTIMAL;
    shadedImageSpec.usage = ImageUsage::STORAGE | ImageUsage::TRANSFER_SRC;
    pass.target = createImage(context, shadedImageSpec);
  }

  Shader computeShader;
  {
    auto csCode = readFileBinary(spec.compFile);
    computeShader = loadSPIRVShader(context, csCode.data(), csCode.size());
  }

  {
    ResourceType types[] = {ResourceType::SAMPLED_IMAGE,
                            ResourceType::SAMPLED_IMAGE,
                            ResourceType::STORAGE_IMAGE};
    ShaderVisibility visibilities[] = {ShaderVisibility::COMPUTE,
                                       ShaderVisibility::COMPUTE,
                                       ShaderVisibility::COMPUTE};

    DescriptorSetLayoutSpecification descriptorSetSpec{};
    descriptorSetSpec.types = std::data(types);
    descriptorSetSpec.visibilities = std::data(visibilities);
    descriptorSetSpec.typeCount = std::size(types);
    pass.setLayout = createDescriptorSetLayout(context, descriptorSetSpec);

    pass.pipelineLayout =
        createPipelineLayout(context, &pass.setLayout, 1, nullptr, 0);

    ComputePipelineSpecification pipelineSpec{};
    pipelineSpec.computeShader = computeShader;
    pipelineSpec.pipelineLayout = pass.pipelineLayout;
    pass.pipeline = createComputePipeline(context, pipelineSpec);
  }

  destroyShader(context, computeShader);

  {
    pass.set =
        createDescriptorSet(context, spec.descriptorPool, pass.setLayout);

    DescriptorUpdateSpecification shadingDescriptorBindings[] = {
        {0, ResourceType::SAMPLED_IMAGE, nullptr, spec.background, nullptr},
        {0, ResourceType::SAMPLED_IMAGE, nullptr, spec.foreground, nullptr},
        {0, ResourceType::STORAGE_IMAGE, nullptr, pass.target, nullptr},
    };

    DescriptorSetUpdateSpecification descriptorSetUpdateSpec{};
    descriptorSetUpdateSpec.descriptorSet = pass.set;
    descriptorSetUpdateSpec.specs = std::data(shadingDescriptorBindings);
    descriptorSetUpdateSpec.specCount = std::size(shadingDescriptorBindings);

    updateDescriptorSet(context, descriptorSetUpdateSpec);
  }

  return pass;
}

void destroyOverlayPass(LContext context, OverlayPass &pass) {
  destroyDescriptorSet(context, pass.set);
  destroyPipeline(context, pass.pipeline);
  destroyPipelineLayout(context, pass.pipelineLayout);
  destroyDescriptorSetLayout(context, pass.setLayout);
  destroyImage(context, pass.target);
}

void recordOverlayPass(LContext context, const OverlayPass &pass,
                       const Scene &scene, DrawCommandIndexes &indexes,
                       DrawCommand &drawCommand) {
  const ImageMetadata targetClearMeta{
      ImageLayout::UNDEFINED, QueueOwnership::GRAPHICS,
      PipelineStage::TOP_OF_PIPE, ResourceAccess::NONE};

  const ImageMetadata computeTargetMeta{
      ImageLayout::GENERAL, QueueOwnership::GRAPHICS,
      PipelineStage::COMPUTE_SHADER, ResourceAccess::SHADER_WRITE};

  drawCommand.operations[indexes.operationIndex++] = {
      DrawOperationType::IMAGE_BARRIER, indexes.imageBarrierIndex};
  drawCommand.imageBarriers[indexes.imageBarrierIndex++] = {
      pass.target, targetClearMeta, computeTargetMeta};

  drawCommand.operations[indexes.operationIndex++] = {
      DrawOperationType::BIND_PIPELINE, indexes.pipelineIndex};
  drawCommand.pipelines[indexes.pipelineIndex++] = pass.pipeline;

  drawCommand.operations[indexes.operationIndex++] = {
      DrawOperationType::BIND_DESCRIPTOR_SETS,
      indexes.descriptorSetBindingIndex};
  drawCommand.descriptorSetBindings[indexes.descriptorSetBindingIndex++] = {
      pass.set, 0};

  drawCommand.operations[indexes.operationIndex++] = {
      DrawOperationType::DISPATCH, indexes.dispatchCallIndex};
  drawCommand.dispatchCalls[indexes.dispatchCallIndex].groupCountX =
      (pass.width + 15) / 16;
  drawCommand.dispatchCalls[indexes.dispatchCallIndex].groupCountY =
      (pass.height + 15) / 16;
  drawCommand.dispatchCalls[indexes.dispatchCallIndex].groupCountZ = 1;
  ++indexes.dispatchCallIndex;
}
