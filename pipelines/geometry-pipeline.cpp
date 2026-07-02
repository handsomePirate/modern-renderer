#include "geometry-pipeline.h"
#include "scene.h"

GeometryPipeline
createGeometryPipeline(LContext context,
                       const GeometryPipelineSpecification &spec) {
  GeometryPipeline pipeline;

  GeometryPassSpecification geometryPassSpec{};
  geometryPassSpec.width = spec.width;
  geometryPassSpec.height = spec.height;
  geometryPassSpec.colorTargetLoadOp = LoadOp::CLEAR;
  geometryPassSpec.depthTargetLoadOp = LoadOp::CLEAR;
  geometryPassSpec.colorPixelFormat = spec.colorPixelFormat;
  geometryPassSpec.depthPixelFormat = spec.depthPixelFormat;
  geometryPassSpec.vertFile = "./shaders/geometry.vert.spv";
  geometryPassSpec.fragFile = "./shaders/geometry.frag.spv";
  pipeline.pass = createGeometryPass(context, geometryPassSpec);

  pipeline.drawProcessor = spec.drawProcessor;

  return pipeline;
}

void destroyGeometryPipeline(LContext context, GeometryPipeline &pipeline) {
  destroyGeometryPass(context, pipeline.pass);
}

void geometryPipelineDrawFrame(LContext context, GeometryPipeline &pipeline,
                               float timeSeconds, DrawCommand &drawCommand,
                               DrawCommandIndexes &indexes, Scene &scene,
                               Image &result, ImageMetadata &resultMeta) {
  const ImageMetadata shaderReadMeta{
      ImageLayout::SHADER_READ_ONLY, QueueOwnership::GRAPHICS,
      PipelineStage::FRAGMENT_SHADER, ResourceAccess::SHADER_READ};

  const ImageMetadata renderTargetColorMeta{
      ImageLayout::COLOR_RENDER_TARGET, QueueOwnership::GRAPHICS,
      PipelineStage::RENDER_TARGET_OUTPUT, ResourceAccess::RENDER_TARGET_WRITE};

  const ImageMetadata renderTargetDepthMeta{
      ImageLayout::DEPTH_RENDER_TARGET, QueueOwnership::GRAPHICS,
      PipelineStage::EARLY_FRAGMENT_TESTS,
      ResourceAccess::DEPTH_STENCIL_READ | ResourceAccess::DEPTH_STENCIL_WRITE};

  resetDrawProcessor(context, pipeline.drawProcessor);

  recordSceneLoadCommands(context, scene, indexes, drawCommand);

  recordGeometryPass(context, pipeline.pass, scene, indexes, drawCommand);

  processDraw(context, pipeline.drawProcessor, drawCommand,
              indexes.operationIndex);

  result = pipeline.pass.colorTarget;
  resultMeta = renderTargetColorMeta;
}
