#include "geometry-pipeline.h"
#include "scene.h"

GeometryPipeline
createGeometryPipeline(svet::renderer::LContext context,
                       const GeometryPipelineSpecification &spec) {
  using namespace svet::renderer;

  GeometryPipeline pipeline;

  GeometryPassSpecification geometryPassSpec{};
  geometryPassSpec.width = spec.width;
  geometryPassSpec.height = spec.height;
  geometryPassSpec.targetImagePool = spec.targetImagePool;
  geometryPassSpec.colorTargetLoadOp = LoadOp::CLEAR;
  geometryPassSpec.depthTargetLoadOp = LoadOp::CLEAR;
  geometryPassSpec.colorPixelFormat = spec.colorPixelFormat;
  geometryPassSpec.depthPixelFormat = spec.depthPixelFormat;
  geometryPassSpec.vertFile = "./shaders/geometry.vert.spv";
  geometryPassSpec.fragFile = "./shaders/geometry.frag.spv";
  pipeline.pass = createGeometryPass(context, geometryPassSpec);

  return pipeline;
}

void destroyGeometryPipeline(svet::renderer::LContext context,
                             GeometryPipeline &pipeline) {
  destroyGeometryPass(context, pipeline.pass);
}

void geometryPipelineDrawFrame(FrameData &frame, GeometryPipeline &pipeline,
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

  resetDrawProcessor(frame.context, frame.drawProcessor);

  recordSceneLoadCommands(frame, scene);

  recordGeometryPass(frame, pipeline.pass, scene);

  submitDrawProcessor(frame.context, frame.drawProcessor);

  result = pipeline.pass.colorTarget;
  resultMeta = renderTargetColorMeta;
}
