#include "deferred-base.h"
#include "file.h"
#include "scene.h"

DeferredBasePass
createDeferredBasePass(svet::renderer::LContext context,
                       const DeferredBasePassSpecification &spec) {
  using namespace svet::renderer;

  DeferredBasePass pass{};

  {
    ImageSpecification graphicsRenderTargetSpec{};
    graphicsRenderTargetSpec.width = spec.width;
    graphicsRenderTargetSpec.height = spec.height;
    graphicsRenderTargetSpec.memoryPool = spec.targetImagePool;
    graphicsRenderTargetSpec.pixelFormat = spec.positionPixelFormat;
    graphicsRenderTargetSpec.usage = ImageUsage::TRANSFER_SRC |
                                     ImageUsage::SAMPLED |
                                     ImageUsage::RENDER_TARGET_COLOR;
    pass.gBuffer[0] = createImage(context, graphicsRenderTargetSpec);
    graphicsRenderTargetSpec.pixelFormat = spec.colorPixelFormat;
    pass.gBuffer[1] = createImage(context, graphicsRenderTargetSpec);
    pass.gBuffer[2] = createImage(context, graphicsRenderTargetSpec);
    pass.gBuffer[3] = createImage(context, graphicsRenderTargetSpec);
  }

  {
    ImageSpecification depthRenderTargetSpec{};
    depthRenderTargetSpec.width = spec.width;
    depthRenderTargetSpec.height = spec.height;
    depthRenderTargetSpec.memoryPool = spec.targetImagePool;
    depthRenderTargetSpec.pixelFormat = spec.depthPixelFormat;
    // We use this image as a render target and blit from it to the swapchain
    depthRenderTargetSpec.usage = ImageUsage::TRANSFER_SRC |
                                  ImageUsage::SAMPLED |
                                  ImageUsage::RENDER_TARGET_DEPTH_STENCIL;
    pass.gBuffer[4] = createImage(context, depthRenderTargetSpec);
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
    ResourceType textureTypes[] = {ResourceType::COMBINED_SAMPLER,
                                   ResourceType::COMBINED_SAMPLER,
                                   ResourceType::COMBINED_SAMPLER};
    ShaderVisibility textureVisibility[] = {ShaderVisibility::FRAGMENT,
                                            ShaderVisibility::FRAGMENT,
                                            ShaderVisibility::FRAGMENT};
    DescriptorSetLayoutSpecification descriptorSetLayoutSpec{};
    descriptorSetLayoutSpec.types = std::data(textureTypes);
    descriptorSetLayoutSpec.visibilities = std::data(textureVisibility);
    descriptorSetLayoutSpec.typeCount = std::size(textureTypes);
    DescriptorSetLayout setLayout =
        createDescriptorSetLayout(context, descriptorSetLayoutSpec);

    ResourceType matType = ResourceType::UNIFORM_BUFFER;
    ShaderVisibility matVis = ShaderVisibility::VERTEX;
    DescriptorSetLayoutSpecification camSetLayoutSpec{};
    camSetLayoutSpec.types = &matType;
    camSetLayoutSpec.visibilities = &matVis;
    camSetLayoutSpec.typeCount = 1;
    DescriptorSetLayout camLayout =
        createDescriptorSetLayout(context, camSetLayoutSpec);

    DescriptorSetLayout setLayouts[] = {camLayout, setLayout};
    PushConstant camPushConstant = {ShaderVisibility::VERTEX, 0,
                                    sizeof(glm::vec3)};
    pass.pipelineLayout =
        createPipelineLayout(context, std::data(setLayouts),
                             std::size(setLayouts), &camPushConstant, 1);

    destroyDescriptorSetLayout(context, camLayout);

    destroyDescriptorSetLayout(context, setLayout);

    PixelFormat pixelFormats[] = {spec.positionPixelFormat,
                                  spec.colorPixelFormat, spec.colorPixelFormat,
                                  spec.colorPixelFormat};

    RenderTargetBlend blends[4]{};
    blends[0].colorWriteMask = ColorComponent::R | ColorComponent::G |
                               ColorComponent::B | ColorComponent::A;
    blends[1].colorWriteMask = ColorComponent::R | ColorComponent::G |
                               ColorComponent::B | ColorComponent::A;
    blends[2].colorWriteMask = ColorComponent::R | ColorComponent::G |
                               ColorComponent::B | ColorComponent::A;
    blends[3].colorWriteMask = ColorComponent::R | ColorComponent::G |
                               ColorComponent::B | ColorComponent::A;

    VertexAttributeDescription attributes[] = {
        {0, 0, 0, VertexFormat::FLOAT3},
        {1, 0, 1, VertexFormat::FLOAT3},
        {2, 0, 2, VertexFormat::FLOAT3},
        {3, 0, 3, VertexFormat::FLOAT2},
    };
    VertexBufferBinding vertexBufferBindings[] = {
        {0, 3 * sizeof(float), VertexInputRate::VERTEX},
        {1, 3 * sizeof(float), VertexInputRate::VERTEX},
        {2, 3 * sizeof(float), VertexInputRate::VERTEX},
        {3, 2 * sizeof(float), VertexInputRate::VERTEX},
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
    pipelineSpec.vertexShader = vertexShader;
    pipelineSpec.fragmentShader = fragmentShader;
    pipelineSpec.pipelineLayout = pass.pipelineLayout;
    pass.pipeline = createGraphicsPipeline(context, pipelineSpec);
  }

  destroyShader(context, vertexShader);
  destroyShader(context, fragmentShader);

  {
    RenderTarget positionTarget{};
    positionTarget.image = pass.gBuffer[0];
    positionTarget.targetLayout = ImageLayout::COLOR_RENDER_TARGET;
    positionTarget.loadOp = LoadOp::CLEAR;
    positionTarget.storeOp = StoreOp::STORE;
    positionTarget.clearColor[0] = 0;
    positionTarget.clearColor[1] = 0;
    positionTarget.clearColor[2] = 0;
    positionTarget.clearColor[3] = 1;

    RenderTarget colorTarget{};
    colorTarget.image = pass.gBuffer[1];
    colorTarget.targetLayout = ImageLayout::COLOR_RENDER_TARGET;
    colorTarget.loadOp = LoadOp::CLEAR;
    colorTarget.storeOp = StoreOp::STORE;
    colorTarget.clearColor[0] = 0;
    colorTarget.clearColor[1] = 0;
    colorTarget.clearColor[2] = 0;
    colorTarget.clearColor[3] = 1;

    RenderTarget normalTarget{};
    normalTarget.image = pass.gBuffer[2];
    normalTarget.targetLayout = ImageLayout::COLOR_RENDER_TARGET;
    normalTarget.loadOp = LoadOp::CLEAR;
    normalTarget.storeOp = StoreOp::STORE;
    normalTarget.clearColor[0] = 0;
    normalTarget.clearColor[1] = 0;
    normalTarget.clearColor[2] = 0;
    normalTarget.clearColor[3] = 1;

    RenderTarget metalroughTarget{};
    metalroughTarget.image = pass.gBuffer[3];
    metalroughTarget.targetLayout = ImageLayout::COLOR_RENDER_TARGET;
    metalroughTarget.loadOp = LoadOp::CLEAR;
    metalroughTarget.storeOp = StoreOp::STORE;
    metalroughTarget.clearColor[0] = 0;
    metalroughTarget.clearColor[1] = 0;
    metalroughTarget.clearColor[2] = 0;
    metalroughTarget.clearColor[3] = 1;

    RenderTarget depthTarget{};
    depthTarget.image = pass.gBuffer[4];
    depthTarget.targetLayout = ImageLayout::DEPTH_RENDER_TARGET;
    depthTarget.loadOp = LoadOp::CLEAR;
    depthTarget.storeOp = StoreOp::STORE;
    depthTarget.clearDepth = 0.f;

    RenderTarget targets[] = {positionTarget, colorTarget, normalTarget,
                              metalroughTarget};

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

void destroyDeferredBasePass(svet::renderer::LContext context,
                             DeferredBasePass &pass) {
  destroyRenderPass(context, pass.renderPass);
  destroyPipeline(context, pass.pipeline);
  destroyPipelineLayout(context, pass.pipelineLayout);
  destroyImage(context, pass.gBuffer[4]);
  destroyImage(context, pass.gBuffer[3]);
  destroyImage(context, pass.gBuffer[2]);
  destroyImage(context, pass.gBuffer[1]);
  destroyImage(context, pass.gBuffer[0]);
}

void recordDeferredBasePass(FrameData &frame, const DeferredBasePass &pass,
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
        {pass.gBuffer[0], targetClearMeta, renderTargetColorMeta},
        {pass.gBuffer[1], targetClearMeta, renderTargetColorMeta},
        {pass.gBuffer[2], targetClearMeta, renderTargetColorMeta},
        {pass.gBuffer[3], targetClearMeta, renderTargetColorMeta},
        {pass.gBuffer[4], targetClearMeta, renderTargetDepthMeta},
    };
    cmdImageBarriers(frame.context, frame.drawProcessor, barriers,
                     std::size(barriers));
    cmdBeginRenderPass(frame.drawProcessor, pass.renderPass);
    cmdBindPipeline(frame.drawProcessor, pass.pipeline);
  }

  {
    cmdBindDescriptorSet(frame.drawProcessor, scene.cameraSet);
    cmdPushConstant(frame.drawProcessor, scene.camera.position,
                    ShaderVisibility::VERTEX);

    auto filterMesh = [&](const SceneMesh &m) -> bool {
      uint32_t positionsIndex = UINT32_MAX;
      uint32_t normalsIndex = UINT32_MAX;
      uint32_t tangentsIndex = UINT32_MAX;
      uint32_t texCoordsIndex = UINT32_MAX;
      for (int i = 0; i < m.vertexAttributes.size(); ++i) {
        if (m.vertexAttributes[i].signature == positionsVASignature) {
          positionsIndex = i;
        } else if (m.vertexAttributes[i].signature == normalsVASignature) {
          normalsIndex = i;
        } else if (m.vertexAttributes[i].signature == tangentsVASignature) {
          tangentsIndex = i;
        } else if (m.vertexAttributes[i].signature == texCoordsVASignature) {
          texCoordsIndex = i;
        }
      }

      const SceneMaterial &material = scene.materials[m.material];
      if (material.renderFlags & (OITMatFlag | ODTMatFlag)) {
        return false;
      }
      uint32_t albedoIndex = UINT32_MAX;
      uint32_t normalIndex = UINT32_MAX;
      uint32_t metalroughIndex = UINT32_MAX;
      for (int i = 0; i < material.descriptors.size(); ++i) {
        if (material.descriptors[i].signature == albedoTexDASignature) {
          albedoIndex = i;
        } else if (material.descriptors[i].signature == normalTexDASignature) {
          normalIndex = i;
        } else if (material.descriptors[i].signature ==
                   metalroughTexDASignature) {
          metalroughIndex = i;
        }
      }

      return positionsIndex != UINT32_MAX && normalsIndex != UINT32_MAX &&
             tangentsIndex != UINT32_MAX && texCoordsIndex != UINT32_MAX &&
             albedoIndex != UINT32_MAX && normalIndex != UINT32_MAX &&
             metalroughIndex != UINT32_MAX &&
             material.descriptors[albedoIndex].descriptorSet ==
                 material.descriptors[normalIndex].descriptorSet &&
             material.descriptors[albedoIndex].descriptorSet ==
                 material.descriptors[metalroughIndex].descriptorSet;
    };

    // TODO: Count the opaque meshes first and allocate only as needed.
    auto meshRefs = (const SceneMesh **)frame.memory.alloc(
        scene.meshes.size() * sizeof(const SceneMesh *),
        alignof(const SceneMesh *));

    int mi = 0;
    for (int i = 0; i < scene.meshes.size(); ++i) {
      if (filterMesh(scene.meshes[i])) {
        meshRefs[mi++] = &scene.meshes[i];
      }
    }
    struct SortByMaterial {
      bool operator()(const SceneMesh *m1, const SceneMesh *m2) {
        return m1->material < m2->material;
      }
    };
    std::sort(meshRefs, meshRefs + mi,
              [](const SceneMesh *m1, const SceneMesh *m2) {
                return m1->material < m2->material;
              });

    uint32_t setIndex = UINT32_MAX;
    int currentMaterial = -1;
    for (int i = 0; i < mi; ++i) {
      uint32_t positionsIndex = UINT32_MAX;
      uint32_t normalsIndex = UINT32_MAX;
      uint32_t tangentsIndex = UINT32_MAX;
      uint32_t texCoordsIndex = UINT32_MAX;
      uint32_t indexesIndex = UINT32_MAX;
      for (int j = 0; j < meshRefs[i]->vertexAttributes.size(); ++j) {
        if (meshRefs[i]->vertexAttributes[j].signature ==
            positionsVASignature) {
          positionsIndex = j;
        } else if (meshRefs[i]->vertexAttributes[j].signature ==
                   normalsVASignature) {
          normalsIndex = j;
        } else if (meshRefs[i]->vertexAttributes[j].signature ==
                   tangentsVASignature) {
          tangentsIndex = j;
        } else if (meshRefs[i]->vertexAttributes[j].signature ==
                   texCoordsVASignature) {
          texCoordsIndex = j;
        } else if (meshRefs[i]->vertexAttributes[j].signature ==
                   indexesVASignature) {
          indexesIndex = j;
        }
      }
      bool drawIndexed = indexesIndex != UINT32_MAX;

      if (currentMaterial < (int)meshRefs[i]->material) {
        const SceneMaterial &material = scene.materials[meshRefs[i]->material];
        for (int j = 0; j < material.descriptors.size(); ++j) {
          if (material.descriptors[j].signature == albedoTexDASignature) {
            setIndex = material.descriptors[j].descriptorSet;
          }
        }
        cmdBindDescriptorSet(frame.drawProcessor,
                             scene.descriptorSets[setIndex], 1);
      }
      currentMaterial = meshRefs[i]->material;

      const auto &posAttr = meshRefs[i]->vertexAttributes[positionsIndex];
      const auto &normAttr = meshRefs[i]->vertexAttributes[normalsIndex];
      const auto &tangAttr = meshRefs[i]->vertexAttributes[tangentsIndex];
      const auto &texCoordsAttr = meshRefs[i]->vertexAttributes[texCoordsIndex];
      VertexBinding vertexBindings[] = {
          {scene.buffers[posAttr.buffer], 0, posAttr.offset},
          {scene.buffers[normAttr.buffer], 1, normAttr.offset},
          {scene.buffers[tangAttr.buffer], 2, tangAttr.offset},
          {scene.buffers[texCoordsAttr.buffer], 3, texCoordsAttr.offset},
      };
      cmdBindVertexAttribs(frame.drawProcessor, vertexBindings,
                           std::size(vertexBindings));

      if (drawIndexed) {
        const auto &indAttr = meshRefs[i]->vertexAttributes[indexesIndex];
        cmdBindIndexBuffer(frame.drawProcessor, scene.buffers[indAttr.buffer],
                           indAttr.offset);
      }

      cmdDrawCall(frame.drawProcessor, meshRefs[i]->elementCount, 1,
                  drawIndexed);
    }
  }

  cmdEndRenderPass(frame.drawProcessor);
}
