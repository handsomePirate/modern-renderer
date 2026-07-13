#include "overlay-pass.h"
#include "file.h"
#include "scene.h"

OverlayPass createOverlayPass(svet::renderer::LContext context,
                              const OverlayPassSpecification &spec) {
  using namespace svet::renderer;

  OverlayPass pass;
  pass.width = spec.width;
  pass.height = spec.height;
  pass.background = spec.background;
  pass.foreground = spec.foreground;

  {
    ImageSpecification shadedImageSpec{};
    shadedImageSpec.width = spec.width;
    shadedImageSpec.height = spec.height;
    shadedImageSpec.memoryPool = spec.targetImagePool;
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

void destroyOverlayPass(svet::renderer::LContext context, OverlayPass &pass) {
  destroyDescriptorSet(context, pass.set);
  destroyPipeline(context, pass.pipeline);
  destroyPipelineLayout(context, pass.pipelineLayout);
  destroyDescriptorSetLayout(context, pass.setLayout);
  destroyImage(context, pass.target);
}

void recordOverlayPass(FrameData &frame, const OverlayPass &pass,
                       const Scene &scene) {
  using namespace svet::renderer;

  const ImageMetadata targetClearMeta{
      ImageLayout::UNDEFINED, QueueOwnershipState::GRAPHICS,
      PipelineStage::TOP_OF_PIPE, ResourceAccess::NONE};

  const ImageMetadata computeTargetMeta{
      ImageLayout::GENERAL, QueueOwnershipState::GRAPHICS,
      PipelineStage::COMPUTE_SHADER, ResourceAccess::SHADER_WRITE};

  cmdImageBarrier(frame.context, frame.drawProcessor, pass.target,
                  targetClearMeta, computeTargetMeta);
  cmdBindPipeline(frame.drawProcessor, pass.pipeline);
  cmdBindDescriptorSet(frame.drawProcessor, pass.set, 0);

  cmdDispatch(frame.drawProcessor, (pass.width + 15) / 16,
              (pass.height + 15) / 16);
}
