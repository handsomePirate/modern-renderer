#include "shadow-pass.h"
#include "file.h"
#include "scene.h"

#include <glm/glm.hpp>

ShadowPass createShadowPass(LContext context,
                            const ShadowPassSpecification &spec) {
  ShadowPass pass{};

  {
    ImageSpecification depthRenderTargetSpec{};
    depthRenderTargetSpec.width = spec.width;
    depthRenderTargetSpec.height = spec.height;
    depthRenderTargetSpec.pixelFormat = spec.depthPixelFormat;
    // We use this image as a render target and blit from it to the swapchain
    depthRenderTargetSpec.usage = ImageUsage::TRANSFER_SRC |
                                  ImageUsage::SAMPLED |
                                  ImageUsage::RENDER_TARGET_DEPTH_STENCIL;
    pass.target = createImage(context, depthRenderTargetSpec);
  }

  Shader vertexShader;
  Shader fragmentShader;
  {
    auto vsCode = readFileBinary(spec.vertFile);
    auto fsCode = readFileBinary(spec.fragFile);
    vertexShader = loadSPIRVShader(context, vsCode.data(), vsCode.size());
    fragmentShader = loadSPIRVShader(context, fsCode.data(), fsCode.size());
  }

  {
    PushConstant camPushConstant = {ShaderVisibility::VERTEX, 0,
                                    sizeof(glm::mat4)};
    pass.pipelineLayout =
        createPipelineLayout(context, nullptr, 0, &camPushConstant, 1);

    VertexAttributeDescription attributes[] = {
        {0, 0, 0, VertexFormat::FLOAT3},
    };
    VertexBufferBinding vertexBufferBindings[] = {
        {0, 3 * sizeof(float), VertexInputRate::VERTEX},
    };

    // Graphics pipeline
    GraphicsPipelineSpecification pipelineSpec{};
    pipelineSpec.width = spec.width;
    pipelineSpec.height = spec.height;
    pipelineSpec.attributes = std::data(attributes);
    pipelineSpec.attributeCount = std::size(attributes);
    pipelineSpec.vertexBindings = std::data(vertexBufferBindings);
    pipelineSpec.vertexBindingCount = std::size(vertexBufferBindings);
    pipelineSpec.drawMode = DrawMode::TRIANGLES;
    pipelineSpec.renderTargetFormats = nullptr;
    pipelineSpec.renderTargetBlends = nullptr;
    pipelineSpec.renderTargetCount = 0;
    pipelineSpec.depthPixelFormat = &spec.depthPixelFormat;
    pipelineSpec.depthMax = 0;
    pipelineSpec.depthMin = 1;
    pipelineSpec.flipViewportY = false;
    pipelineSpec.enableDepthTest = true;
    pipelineSpec.enableDepthWrite = true;
    pipelineSpec.frontFaceWind = FrontFaceWind::CLOCKWISE;
    // Only back faces cast shadows to prevent shadow acne
    pipelineSpec.faceCulling = FaceCulling::FRONT;
    pipelineSpec.vertexShader = vertexShader;
    pipelineSpec.fragmentShader = fragmentShader;
    pipelineSpec.pipelineLayout = pass.pipelineLayout;
    pass.pipeline = createGraphicsPipeline(context, pipelineSpec);
  }

  destroyShader(context, vertexShader);
  destroyShader(context, fragmentShader);

  {
    RenderTarget depthTarget{};
    depthTarget.image = pass.target;
    depthTarget.targetLayout = ImageLayout::DEPTH_RENDER_TARGET;
    depthTarget.loadOp = LoadOp::CLEAR;
    depthTarget.storeOp = StoreOp::STORE;
    depthTarget.clearDepth = 0.f;

    RenderPassSpecification renderPassSpec{};
    renderPassSpec.width = spec.width;
    renderPassSpec.height = spec.height;
    renderPassSpec.renderTargets = nullptr;
    renderPassSpec.renderTargetCount = 0;
    renderPassSpec.depthTarget = &depthTarget;
    pass.renderPass = createRenderPass(context, renderPassSpec);
  }

  return pass;
}

void destroyShadowPass(LContext context, ShadowPass &pass) {
  destroyRenderPass(context, pass.renderPass);
  destroyPipeline(context, pass.pipeline);
  destroyPipelineLayout(context, pass.pipelineLayout);
  destroyImage(context, pass.target);
}

void recordShadowPass(LContext context, const ShadowPass &pass,
                      const Scene &scene, DrawCommandIndexes &indexes,
                      DrawCommand &drawCommand) {
  {
    const ImageMetadata targetClearMeta{
        ImageLayout::UNDEFINED, QueueOwnership::GRAPHICS,
        PipelineStage::FRAGMENT_SHADER, ResourceAccess::SHADER_READ};

    const ImageMetadata renderTargetDepthMeta{
        ImageLayout::DEPTH_RENDER_TARGET, QueueOwnership::GRAPHICS,
        PipelineStage::EARLY_FRAGMENT_TESTS,
        ResourceAccess::DEPTH_STENCIL_READ |
            ResourceAccess::DEPTH_STENCIL_WRITE};

    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::IMAGE_BARRIER, indexes.imageBarrierIndex};
    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::BEGIN_RENDER_PASS, indexes.renderPassIndex};
    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::BIND_PIPELINE, indexes.pipelineIndex};

    drawCommand.imageBarriers[indexes.imageBarrierIndex++] = {
        pass.target, targetClearMeta, renderTargetDepthMeta};

    drawCommand.renderPasses[indexes.renderPassIndex] = pass.renderPass;
    drawCommand.pipelines[indexes.pipelineIndex++] = pass.pipeline;
  }

  {
    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::PUSH_CONSTANT, indexes.pushConstantIndex};
    drawCommand.pushConstants[indexes.pushConstantIndex].visibility =
        ShaderVisibility::VERTEX;
    auto orto =
        (glm::mat4 *)drawCommand.pushConstants[indexes.pushConstantIndex].bytes;
    *orto = getSunOrtho(scene.sun);
    drawCommand.pushConstants[indexes.pushConstantIndex].offset = 0;
    drawCommand.pushConstants[indexes.pushConstantIndex].size =
        sizeof(glm::mat4);
    ++indexes.pushConstantIndex;

    for (int i = 0; i < scene.meshes.size(); ++i) {
      uint32_t positionsIndex = UINT32_MAX;
      uint32_t indexesIndex = UINT32_MAX;
      for (int j = 0; j < scene.meshes[i].vertexAttributes.size(); ++j) {
        if (scene.meshes[i].vertexAttributes[j].signature ==
            positionsVASignature) {
          positionsIndex = j;
        } else if (scene.meshes[i].vertexAttributes[j].signature ==
                   indexesVASignature) {
          indexesIndex = j;
        }
      }
      if (positionsIndex == UINT32_MAX ||
          // Alpha-blended geometry should not cast and binary transparency
          // usually has only thin surfaces with no backfaces so they will not
          // cast anyway - no need to process them
          scene.materials[scene.meshes[i].material].renderFlags &
              (alphaBlendedMatFlag | binaryTransparencyMatFlag)) {
        continue;
      }

      bool drawIndexed = indexesIndex != UINT32_MAX;

      drawCommand.operations[indexes.operationIndex++] = {
          DrawOperationType::BIND_VERTEX_BUFFER, indexes.vertexBindingIndex, 1};
      const auto &posAttr = scene.meshes[i].vertexAttributes[positionsIndex];
      drawCommand.vertexBindings[indexes.vertexBindingIndex++] = {
          scene.buffers[posAttr.buffer], 0, posAttr.offset};

      if (drawIndexed) {
        drawCommand.operations[indexes.operationIndex++] = {
            DrawOperationType::BIND_INDEX_BUFFER, indexes.indexBindingIndex};

        const auto &indAttr = scene.meshes[i].vertexAttributes[indexesIndex];
        drawCommand.indexBindings[indexes.indexBindingIndex++] = {
            scene.buffers[indAttr.buffer], indAttr.offset};
      }

      drawCommand.operations[indexes.operationIndex++] = {
          DrawOperationType::DRAW, indexes.drawCallIndex};
      drawCommand.drawCalls[indexes.drawCallIndex++] = {
          scene.meshes[i].elementCount, 1, drawIndexed};
    }
  }

  drawCommand.operations[indexes.operationIndex++] = {
      DrawOperationType::END_RENDER_PASS, indexes.renderPassIndex++};
}
