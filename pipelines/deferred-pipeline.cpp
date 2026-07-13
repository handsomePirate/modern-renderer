#include "deferred-pipeline.h"
#include "scene.h"

DeferredPipeline
createDeferredPipeline(svet::renderer::LContext context,
                       const DeferredPipelineSpecification &spec) {
  using namespace svet::renderer;

  DeferredPipeline pipeline;

  ShadowPassSpecification shadowPassSpec{};
  shadowPassSpec.width = spec.shadowMapResolution;
  shadowPassSpec.height = spec.shadowMapResolution;
  shadowPassSpec.targetImagePool = spec.targetImagePool;
  shadowPassSpec.depthPixelFormat = spec.shadowMapPixelFormat;
  shadowPassSpec.vertFile = "./shaders/shadow.vert.spv";
  shadowPassSpec.fragFile = "./shaders/shadow.frag.spv";
  pipeline.shadowPass = createShadowPass(context, shadowPassSpec);

  DeferredBasePassSpecification deferredBasePassSpec{};
  deferredBasePassSpec.width = spec.width;
  deferredBasePassSpec.height = spec.height;
  deferredBasePassSpec.targetImagePool = spec.targetImagePool;
  deferredBasePassSpec.colorPixelFormat = spec.colorPixelFormat;
  deferredBasePassSpec.positionPixelFormat = spec.positionPixelFormat;
  deferredBasePassSpec.depthPixelFormat = spec.depthPixelFormat;
  deferredBasePassSpec.vertFile = "./shaders/deferred-base.vert.spv";
  deferredBasePassSpec.fragFile = "./shaders/deferred-base.frag.spv";
  pipeline.basePass = createDeferredBasePass(context, deferredBasePassSpec);

  DeferredShadingPassSpecification deferredShadingPassSpec{};
  deferredShadingPassSpec.uniformBufferPool = spec.uniformBufferPool;
  deferredShadingPassSpec.targetImagePool = spec.targetImagePool;
  deferredShadingPassSpec.descriptorPool = spec.descriptorPool;
  deferredShadingPassSpec.gBuffer[0] = pipeline.basePass.gBuffer[0];
  deferredShadingPassSpec.gBuffer[1] = pipeline.basePass.gBuffer[1];
  deferredShadingPassSpec.gBuffer[2] = pipeline.basePass.gBuffer[2];
  deferredShadingPassSpec.gBuffer[3] = pipeline.basePass.gBuffer[3];
  deferredShadingPassSpec.gBuffer[4] = pipeline.basePass.gBuffer[4];
  deferredShadingPassSpec.shadowMap = pipeline.shadowPass.target;
  deferredShadingPassSpec.width = spec.width;
  deferredShadingPassSpec.height = spec.height;
  deferredShadingPassSpec.shadowMapResolution = spec.shadowMapResolution;
  deferredShadingPassSpec.colorPixelFormat = spec.colorPixelFormat;
  deferredShadingPassSpec.compFile = "./shaders/deferred-shade.comp.spv";
  pipeline.shadingPass =
      createDeferredShadingPass(context, deferredShadingPassSpec);

  // TODO: This is more general than it needs to be. If we only have binary
  // alpha blends, we can use a cheaper alpha-to-coverage
  WeightedOITSpecification weightedOITPassSpec{};
  weightedOITPassSpec.width = spec.width;
  weightedOITPassSpec.height = spec.height;
  weightedOITPassSpec.shadowMapResolution = spec.shadowMapResolution;
  weightedOITPassSpec.uniformBufferPool = spec.uniformBufferPool;
  weightedOITPassSpec.targetImagePool = spec.targetImagePool;
  weightedOITPassSpec.descriptorPool = spec.descriptorPool;
  weightedOITPassSpec.accumulatorFormat = PixelFormat::FLOAT16_RGBA;
  weightedOITPassSpec.revealFormat = PixelFormat::FLOAT16_R;
  weightedOITPassSpec.colorPixelFormat = spec.colorPixelFormat;
  weightedOITPassSpec.depthPixelFormat = spec.depthPixelFormat;
  weightedOITPassSpec.inheritedDepth = pipeline.basePass.gBuffer[4];
  weightedOITPassSpec.shadowMap = pipeline.shadowPass.target;
  weightedOITPassSpec.vertFile = "./shaders/weighted-oit.vert.spv";
  weightedOITPassSpec.fragFile = "./shaders/weighted-oit.frag.spv";
  weightedOITPassSpec.compFile = "./shaders/weighted-oit-resolve.comp.spv";
  pipeline.weightedOITPass =
      createWeightedOITPass(context, weightedOITPassSpec);

  OverlayPassSpecification overlayPassSpec{};
  overlayPassSpec.width = spec.width;
  overlayPassSpec.height = spec.height;
  overlayPassSpec.targetImagePool = spec.targetImagePool;
  overlayPassSpec.colorPixelFormat = spec.colorPixelFormat;
  overlayPassSpec.descriptorPool = spec.descriptorPool;
  overlayPassSpec.background = pipeline.shadingPass.target;
  overlayPassSpec.foreground = pipeline.weightedOITPass.resolve;
  overlayPassSpec.compFile = "./shaders/overlay.comp.spv";
  pipeline.overlayPass = createOverlayPass(context, overlayPassSpec);

  return pipeline;
}

void destroyDeferredPipeline(svet::renderer::LContext context,
                             DeferredPipeline &pipeline) {
  destroyOverlayPass(context, pipeline.overlayPass);
  destroyWeightedOITPass(context, pipeline.weightedOITPass);
  destroyDeferredShadingPass(context, pipeline.shadingPass);
  destroyDeferredBasePass(context, pipeline.basePass);
  destroyShadowPass(context, pipeline.shadowPass);
}

void deferredPipelineDrawFrame(FrameData &frame, DeferredPipeline &pipeline,
                               Scene &scene, svet::renderer::Image &result,
                               svet::renderer::ImageMetadata &resultMeta) {
  using namespace svet::renderer;

  const ImageMetadata shaderReadMeta{
      ImageLayout::SHADER_READ_ONLY, QueueOwnershipState::GRAPHICS,
      PipelineStage::FRAGMENT_SHADER, ResourceAccess::SHADER_READ};

  const ImageMetadata renderTargetColorMeta{
      ImageLayout::COLOR_RENDER_TARGET, QueueOwnershipState::GRAPHICS,
      PipelineStage::RENDER_TARGET_OUTPUT, ResourceAccess::RENDER_TARGET_WRITE};

  const ImageMetadata renderTargetDepthMeta{
      ImageLayout::DEPTH_RENDER_TARGET, QueueOwnershipState::GRAPHICS,
      PipelineStage::EARLY_FRAGMENT_TESTS,
      ResourceAccess::DEPTH_STENCIL_READ | ResourceAccess::DEPTH_STENCIL_WRITE};

  const ImageMetadata computeTargetMeta{
      ImageLayout::GENERAL, QueueOwnershipState::GRAPHICS,
      PipelineStage::COMPUTE_SHADER, ResourceAccess::SHADER_WRITE};

  resetDrawProcessor(frame.context, frame.drawProcessor);

  recordSceneLoadCommands(frame, scene);

  recordShadowPass(frame, pipeline.shadowPass, scene);

  recordDeferredBasePass(frame, pipeline.basePass, scene);

  {
    ImageBarrier barriers[] = {
        {pipeline.basePass.gBuffer[0], renderTargetColorMeta, shaderReadMeta},
        {pipeline.basePass.gBuffer[1], renderTargetColorMeta, shaderReadMeta},
        {pipeline.basePass.gBuffer[2], renderTargetColorMeta, shaderReadMeta},
        {pipeline.basePass.gBuffer[3], renderTargetColorMeta, shaderReadMeta},
        {pipeline.basePass.gBuffer[4], renderTargetDepthMeta, shaderReadMeta},
        {pipeline.shadowPass.target, renderTargetDepthMeta, shaderReadMeta},
    };
    cmdImageBarriers(frame.context, frame.drawProcessor, barriers,
                     std::size(barriers));
  }

  recordDeferredShadingPass(frame, pipeline.shadingPass, scene);

  cmdImageBarrier(frame.context, frame.drawProcessor,
                  pipeline.basePass.gBuffer[4], shaderReadMeta,
                  renderTargetDepthMeta);

  recordWeightedOITPassDrawScene(frame, pipeline.weightedOITPass, scene);

  {
    ImageBarrier barriers[] = {
        {pipeline.overlayPass.background, computeTargetMeta, shaderReadMeta},
        {pipeline.overlayPass.foreground, computeTargetMeta, shaderReadMeta},
    };
    cmdImageBarriers(frame.context, frame.drawProcessor, barriers,
                     std::size(barriers));
  }

  recordOverlayPass(frame, pipeline.overlayPass, scene);

  submitDrawProcessor(frame.context, frame.drawProcessor);

  result = pipeline.overlayPass.target;
  resultMeta = computeTargetMeta;
}
