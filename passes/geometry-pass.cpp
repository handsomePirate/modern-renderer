#include "geometry-pass.h"
#include "file.h"
#include "scene.h"

GeometryPass createGeometryPass(svet::renderer::LContext context,
                                const GeometryPassSpecification &spec) {
  using namespace svet::renderer;

  GeometryPass pass{};

  if (spec.colorTarget) {
    pass.colorTarget = spec.colorTarget;
    pass.shouldDestroyColorTarget = false;
  } else {
    ImageSpecification graphicsRenderTargetSpec{};
    graphicsRenderTargetSpec.width = spec.width;
    graphicsRenderTargetSpec.height = spec.height;
    graphicsRenderTargetSpec.memoryPool = spec.targetImagePool;
    graphicsRenderTargetSpec.pixelFormat = spec.colorPixelFormat;
    graphicsRenderTargetSpec.usage = ImageUsage::TRANSFER_SRC |
                                     ImageUsage::SAMPLED |
                                     ImageUsage::RENDER_TARGET_COLOR;
    pass.colorTarget = createImage(context, graphicsRenderTargetSpec);
    pass.shouldDestroyColorTarget = true;
  }

  if (spec.depthTarget) {
    pass.depthTarget = spec.depthTarget;
    pass.shouldDestroyDepthTarget = false;
  } else {
    ImageSpecification depthRenderTargetSpec{};
    depthRenderTargetSpec.width = spec.width;
    depthRenderTargetSpec.height = spec.height;
    depthRenderTargetSpec.memoryPool = spec.targetImagePool;
    depthRenderTargetSpec.pixelFormat = spec.depthPixelFormat;
    // We use this image as a render target and blit from it to the swapchain
    depthRenderTargetSpec.usage = ImageUsage::TRANSFER_SRC |
                                  ImageUsage::SAMPLED |
                                  ImageUsage::RENDER_TARGET_DEPTH_STENCIL;
    pass.depthTarget = createImage(context, depthRenderTargetSpec);
    pass.shouldDestroyDepthTarget = true;
    ;
  }

  // Shaders
  Shader vertexShader;
  Shader fragmentShader;
  {
    auto vsCode = readFileBinary(spec.vertFile);
    auto fsCode = readFileBinary(spec.fragFile);
    vertexShader = loadSPIRVShader(context, vsCode.data(), vsCode.size());
    fragmentShader = loadSPIRVShader(context, fsCode.data(), fsCode.size());
  }

  {
    ResourceType matType = ResourceType::UNIFORM_BUFFER;
    ShaderVisibility matVis = ShaderVisibility::VERTEX;
    DescriptorSetLayoutSpecification camSetLayoutSpec{};
    camSetLayoutSpec.types = &matType;
    camSetLayoutSpec.visibilities = &matVis;
    camSetLayoutSpec.typeCount = 1;
    DescriptorSetLayout camLayout =
        createDescriptorSetLayout(context, camSetLayoutSpec);

    // Graphics pipeline layout
    DescriptorSetLayout setLayouts[] = {camLayout};
    pass.pipelineLayout = createPipelineLayout(
        context, std::data(setLayouts), std::size(setLayouts), nullptr, 0);

    destroyDescriptorSetLayout(context, camLayout);

    PixelFormat pixelFormats[] = {spec.colorPixelFormat};

    RenderTargetBlend blends[1]{};
    blends[0].colorWriteMask = ColorComponent::R | ColorComponent::G |
                               ColorComponent::B | ColorComponent::A;

    VertexAttributeDescription attributes[] = {
        {0, 0, 0, VertexFormat::FLOAT3},
        {1, 0, 1, VertexFormat::FLOAT3},
    };
    VertexBufferBinding vertexBufferBindings[] = {
        {0, 3 * sizeof(float), VertexInputRate::VERTEX},
        {1, 3 * sizeof(float), VertexInputRate::VERTEX},
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
    pipelineSpec.renderTargetFormats = std::data(pixelFormats);
    pipelineSpec.renderTargetBlends = blends;
    pipelineSpec.renderTargetCount = std::size(pixelFormats);
    pipelineSpec.depthPixelFormat = &spec.depthPixelFormat;
    pipelineSpec.depthMax = 0;
    pipelineSpec.depthMin = 1;
    pipelineSpec.flipViewportY = true;
    pipelineSpec.enableDepthTest = true;
    pipelineSpec.enableDepthWrite = true;
    pipelineSpec.frontFaceWind = FrontFaceWind::COUNTER_CLOCKWISE;
    pipelineSpec.faceCulling = FaceCulling::BACK;
    pipelineSpec.multiSampleCount = 1;
    pipelineSpec.vertexShader = vertexShader;
    pipelineSpec.fragmentShader = fragmentShader;
    pipelineSpec.pipelineLayout = pass.pipelineLayout;
    pipelineSpec.enableAlphaToCoverage = false;
    pass.pipeline = createGraphicsPipeline(context, pipelineSpec);
  }

  destroyShader(context, vertexShader);
  destroyShader(context, fragmentShader);

  {
    RenderTarget colorTarget{};
    colorTarget.image = pass.colorTarget;
    colorTarget.targetLayout = ImageLayout::COLOR_RENDER_TARGET;
    colorTarget.loadOp = spec.colorTargetLoadOp;
    colorTarget.storeOp = StoreOp::STORE;
    colorTarget.clearColor[0] = 0;
    colorTarget.clearColor[1] = 0;
    colorTarget.clearColor[2] = 0;
    colorTarget.clearColor[3] = 1;

    RenderTarget depthTarget{};
    depthTarget.image = pass.depthTarget;
    depthTarget.targetLayout = ImageLayout::DEPTH_RENDER_TARGET;
    depthTarget.loadOp = spec.depthTargetLoadOp;
    depthTarget.storeOp = StoreOp::STORE;
    depthTarget.clearDepth = 0.f;

    RenderTarget targets[] = {colorTarget};

    RenderPassSpecification renderPassSpec{};
    renderPassSpec.width = spec.width;
    renderPassSpec.height = spec.height;
    renderPassSpec.renderTargets = std::data(targets);
    renderPassSpec.renderTargetCount = std::size(targets);
    renderPassSpec.depthTarget = &depthTarget;
    pass.renderPass = createRenderPass(context, renderPassSpec);
  }

  return pass;
}

void destroyGeometryPass(svet::renderer::LContext context, GeometryPass &pass) {
  destroyRenderPass(context, pass.renderPass);
  destroyPipeline(context, pass.pipeline);
  destroyPipelineLayout(context, pass.pipelineLayout);
  if (pass.shouldDestroyDepthTarget)
    destroyImage(context, pass.depthTarget);
  if (pass.shouldDestroyColorTarget)
    destroyImage(context, pass.colorTarget);
}

void recordGeometryPass(FrameData &frame, const GeometryPass &pass,
                        const Scene &scene) {
  using namespace svet::renderer;

  {
    const ImageMetadata targetClearMeta{
        ImageLayout::UNDEFINED, QueueOwnershipState::GRAPHICS,
        PipelineStage::FRAGMENT_SHADER, ResourceAccess::SHADER_READ};

    const ImageMetadata renderTargetColorMeta{
        ImageLayout::COLOR_RENDER_TARGET, QueueOwnershipState::GRAPHICS,
        PipelineStage::RENDER_TARGET_OUTPUT,
        ResourceAccess::RENDER_TARGET_WRITE};

    const ImageMetadata renderTargetDepthMeta{
        ImageLayout::DEPTH_RENDER_TARGET, QueueOwnershipState::GRAPHICS,
        PipelineStage::EARLY_FRAGMENT_TESTS,
        ResourceAccess::DEPTH_STENCIL_READ |
            ResourceAccess::DEPTH_STENCIL_WRITE};

    ImageBarrier barriers[] = {
        {pass.colorTarget, targetClearMeta, renderTargetColorMeta},
        {pass.depthTarget, targetClearMeta, renderTargetDepthMeta},
    };
    cmdImageBarriers(frame.context, frame.drawProcessor, barriers,
                     std::size(barriers));
    cmdBeginRenderPass(frame.drawProcessor, pass.renderPass);
    cmdBindPipeline(frame.drawProcessor, pass.pipeline);
  }

  {
    cmdBindDescriptorSet(frame.drawProcessor, scene.cameraSet);

    for (int i = 0; i < scene.meshes.size(); ++i) {
      uint32_t positionsIndex = UINT32_MAX;
      uint32_t normalsIndex = UINT32_MAX;
      uint32_t indexesIndex = UINT32_MAX;
      for (int j = 0; j < scene.meshes[i].vertexAttributes.size(); ++j) {
        if (scene.meshes[i].vertexAttributes[j].signature ==
            positionsVASignature) {
          positionsIndex = j;
        } else if (scene.meshes[i].vertexAttributes[j].signature ==
                   normalsVASignature) {
          normalsIndex = j;
        } else if (scene.meshes[i].vertexAttributes[j].signature ==
                   indexesVASignature) {
          indexesIndex = j;
        }
      }
      bool shouldDraw =
          positionsIndex != UINT32_MAX && normalsIndex != UINT32_MAX;
      bool isIndexedDraw = indexesIndex != UINT32_MAX;
      if (shouldDraw) {
        const auto &posAttr = scene.meshes[i].vertexAttributes[positionsIndex];
        const auto &normAttr = scene.meshes[i].vertexAttributes[normalsIndex];
        VertexBinding vertexBindings[] = {
            {scene.buffers[posAttr.buffer], 0, posAttr.offset},
            {scene.buffers[normAttr.buffer], 1, normAttr.offset},
        };
        cmdBindVertexAttribs(frame.drawProcessor, vertexBindings,
                             std::size(vertexBindings));

        if (isIndexedDraw) {
          const auto &indAttr = scene.meshes[i].vertexAttributes[indexesIndex];
          cmdBindIndexBuffer(frame.drawProcessor, scene.buffers[indAttr.buffer],
                             indAttr.offset);
        }

        cmdDrawCall(frame.drawProcessor, scene.meshes[i].elementCount,
                    scene.meshes[i].instanceCount, isIndexedDraw);
      }
    }
  }

  cmdEndRenderPass(frame.drawProcessor);
}
