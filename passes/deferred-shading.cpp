#include "deferred-shading.h"
#include "file.h"
#include "scene.h"

#include <glm/glm.hpp>

DeferredShadingPass
createDeferredShadingPass(svet::renderer::LContext context,
                          const DeferredShadingPassSpecification &spec) {
  using namespace svet::renderer;

  DeferredShadingPass pass;
  pass.width = spec.width;
  pass.height = spec.height;
  pass.shadowMapResolution = spec.shadowMapResolution;

  {
    BufferSpecification bufferSpec{};
    bufferSpec.size = sizeof(DeferredShadingParams);
    bufferSpec.memoryPool = spec.uniformBufferPool;
    bufferSpec.usage = BufferUsage::UNIFORM;
    bufferSpec.initialOwnership = QueueOwnership::GRAPHICS;
    pass.shadingParamsBuffer = createBuffer(context, bufferSpec);
  }

  {
    ImageSpecification shadedImageSpec{};
    shadedImageSpec.width = spec.width;
    shadedImageSpec.height = spec.height;
    shadedImageSpec.memoryPool = spec.targetImagePool;
    shadedImageSpec.pixelFormat = spec.colorPixelFormat;
    shadedImageSpec.tiling = ImageTiling::OPTIMAL;
    shadedImageSpec.usage =
        ImageUsage::SAMPLED | ImageUsage::STORAGE | ImageUsage::TRANSFER_SRC;
    pass.target = createImage(context, shadedImageSpec);
  }

  Shader computeShader;
  {
    auto csCode = readFileBinary(spec.compFile);
    computeShader = loadSPIRVShader(context, csCode.data(), csCode.size());
  }

  {
    ResourceType types[] = {
        ResourceType::UNIFORM_BUFFER, ResourceType::SAMPLED_IMAGE,
        ResourceType::SAMPLED_IMAGE,  ResourceType::SAMPLED_IMAGE,
        ResourceType::SAMPLED_IMAGE,  ResourceType::SAMPLED_IMAGE,
        ResourceType::SAMPLED_IMAGE,  ResourceType::STORAGE_IMAGE};
    ShaderVisibility visibilities[] = {
        ShaderVisibility::COMPUTE, ShaderVisibility::COMPUTE,
        ShaderVisibility::COMPUTE, ShaderVisibility::COMPUTE,
        ShaderVisibility::COMPUTE, ShaderVisibility::COMPUTE,
        ShaderVisibility::COMPUTE, ShaderVisibility::COMPUTE};

    DescriptorSetLayoutSpecification descriptorSetSpec{};
    descriptorSetSpec.types = std::data(types);
    descriptorSetSpec.visibilities = std::data(visibilities);
    descriptorSetSpec.typeCount = std::size(types);
    pass.gBufferSetLayout =
        createDescriptorSetLayout(context, descriptorSetSpec);

    PushConstant modelSelectConstant = {ShaderVisibility::COMPUTE, 0,
                                        sizeof(uint32_t)};

    pass.pipelineLayout = createPipelineLayout(context, &pass.gBufferSetLayout,
                                               1, &modelSelectConstant, 1);

    ComputePipelineSpecification pipelineSpec{};
    pipelineSpec.computeShader = computeShader;
    pipelineSpec.pipelineLayout = pass.pipelineLayout;
    pass.pipeline = createComputePipeline(context, pipelineSpec);
  }

  destroyShader(context, computeShader);

  {
    pass.gBufferSet = createDescriptorSet(context, spec.descriptorPool,
                                          pass.gBufferSetLayout);

    DescriptorUpdateSpecification shadingDescriptorBindings[] = {
        {0, ResourceType::UNIFORM_BUFFER, pass.shadingParamsBuffer, nullptr,
         nullptr},
        {0, ResourceType::SAMPLED_IMAGE, nullptr, spec.gBuffer[0], nullptr},
        {0, ResourceType::SAMPLED_IMAGE, nullptr, spec.gBuffer[1], nullptr},
        {0, ResourceType::SAMPLED_IMAGE, nullptr, spec.gBuffer[2], nullptr},
        {0, ResourceType::SAMPLED_IMAGE, nullptr, spec.gBuffer[3], nullptr},
        {0, ResourceType::SAMPLED_IMAGE, nullptr, spec.gBuffer[4], nullptr},
        {0, ResourceType::SAMPLED_IMAGE, nullptr, spec.shadowMap, nullptr},
        {0, ResourceType::STORAGE_IMAGE, nullptr, pass.target, nullptr},
    };

    DescriptorSetUpdateSpecification descriptorSetUpdateSpec{};
    descriptorSetUpdateSpec.descriptorSet = pass.gBufferSet;
    descriptorSetUpdateSpec.specs = std::data(shadingDescriptorBindings);
    descriptorSetUpdateSpec.specCount = std::size(shadingDescriptorBindings);

    updateDescriptorSet(context, descriptorSetUpdateSpec);
  }

  return pass;
}

void destroyDeferredShadingPass(svet::renderer::LContext context,
                                DeferredShadingPass &pass) {
  destroyDescriptorSet(context, pass.gBufferSet);
  destroyPipeline(context, pass.pipeline);
  destroyPipelineLayout(context, pass.pipelineLayout);
  destroyDescriptorSetLayout(context, pass.gBufferSetLayout);
  destroyImage(context, pass.target);
  destroyBuffer(context, pass.shadingParamsBuffer);
}

void recordDeferredShadingPass(FrameData &frame,
                               const DeferredShadingPass &pass,
                               const Scene &scene) {
  using namespace svet::renderer;

  DeferredShadingParams params{};
  params.sunOrto = getSunOrtho(scene.sun);
  params.camPos = scene.camera.position;
  params.sunDir = scene.sun.direction;
  params.sunIntensity = scene.sun.intensity;
  params.shadowMapRes = pass.shadowMapResolution;
  params.sunCol = scene.sun.color;
  uploadBufferData(frame.context, pass.shadingParamsBuffer, &params, 0,
                   sizeof(DeferredShadingParams));
  const ImageMetadata targetClearMeta{
      ImageLayout::UNDEFINED, QueueOwnershipState::GRAPHICS,
      PipelineStage::TOP_OF_PIPE, ResourceAccess::NONE};

  const ImageMetadata computeTargetMeta{
      ImageLayout::GENERAL, QueueOwnershipState::GRAPHICS,
      PipelineStage::COMPUTE_SHADER, ResourceAccess::SHADER_WRITE};

  cmdImageBarrier(frame.context, frame.drawProcessor, pass.target,
                  targetClearMeta, computeTargetMeta);
  cmdBindPipeline(frame.drawProcessor, pass.pipeline);
  cmdBindDescriptorSet(frame.drawProcessor, pass.gBufferSet);
  cmdPushConstant(frame.drawProcessor, 2u, ShaderVisibility::COMPUTE);

  cmdDispatch(frame.drawProcessor, (pass.width + 15) / 16,
              (pass.height + 15) / 16);
}
