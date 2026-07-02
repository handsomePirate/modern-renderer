#include "weighted-oit.h"
#include "deferred-shading.h"
#include "file.h"
#include "scene.h"

WeightedOITPass createWeightedOITPass(LContext context,
                                      const WeightedOITSpecification &spec) {
  WeightedOITPass pass{};
  pass.width = spec.width;
  pass.height = spec.height;
  pass.inheritedDepth = spec.inheritedDepth;
  pass.shadowMapResolution = spec.shadowMapResolution;

  {
    BufferSpecification bufferSpec{};
    bufferSpec.size = sizeof(DeferredShadingParams);
    bufferSpec.source = BufferSource::CPU;
    bufferSpec.consumer = BufferConsumer::GPU;
    bufferSpec.frequency = BufferFrequency::DYNAMIC;
    bufferSpec.usage = BufferUsage::UNIFORM;
    bufferSpec.initialOwnership = QueueOwnership::GRAPHICS;
    pass.shadingParamsBuffer = allocateBuffer(context, bufferSpec);
  }

  {
    ImageSpecification graphicsRenderTargetSpec{};
    graphicsRenderTargetSpec.width = spec.width;
    graphicsRenderTargetSpec.height = spec.height;
    graphicsRenderTargetSpec.pixelFormat = spec.accumulatorFormat;
    graphicsRenderTargetSpec.usage = ImageUsage::TRANSFER_SRC |
                                     ImageUsage::SAMPLED |
                                     ImageUsage::RENDER_TARGET_COLOR;
    pass.accumulator = createImage(context, graphicsRenderTargetSpec);
    graphicsRenderTargetSpec.pixelFormat = spec.revealFormat;
    pass.reveal = createImage(context, graphicsRenderTargetSpec);
  }

  {
    ImageSpecification computeRenderTargetSpec{};
    computeRenderTargetSpec.width = spec.width;
    computeRenderTargetSpec.height = spec.height;
    computeRenderTargetSpec.pixelFormat = spec.colorPixelFormat;
    computeRenderTargetSpec.usage =
        ImageUsage::TRANSFER_SRC | ImageUsage::SAMPLED | ImageUsage::STORAGE;
    pass.resolve = createImage(context, computeRenderTargetSpec);
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
    // TODO: Handle inherited color
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

    ResourceType shadingParamsTypes[] = {ResourceType::UNIFORM_BUFFER,
                                         ResourceType::SAMPLED_IMAGE};
    ShaderVisibility shadingParamsVis[] = {ShaderVisibility::FRAGMENT,
                                           ShaderVisibility::FRAGMENT};
    DescriptorSetLayoutSpecification shadingSetLayoutSpec{};
    shadingSetLayoutSpec.types = std::data(shadingParamsTypes);
    shadingSetLayoutSpec.visibilities = std::data(shadingParamsVis);
    shadingSetLayoutSpec.typeCount = std::size(shadingParamsTypes);
    pass.shadingLayout =
        createDescriptorSetLayout(context, shadingSetLayoutSpec);

    DescriptorSetLayout setLayouts[] = {camLayout, setLayout,
                                        pass.shadingLayout};
    PushConstant camPushConstant = {ShaderVisibility::FRAGMENT, 0,
                                    sizeof(glm::vec3)};
    pass.graphicsPipelineLayout =
        createPipelineLayout(context, std::data(setLayouts),
                             std::size(setLayouts), &camPushConstant, 1);

    destroyDescriptorSetLayout(context, camLayout);

    destroyDescriptorSetLayout(context, setLayout);

    PixelFormat pixelFormats[] = {spec.accumulatorFormat, spec.revealFormat};

    RenderTargetBlend blends[2]{};

    blends[0].allow = true;
    blends[0].colorOp = BlendOp::ADD;
    blends[0].srcColorFactor = BlendFactor::ONE;
    blends[0].dstColorFactor = BlendFactor::ONE;
    blends[0].alphaOp = BlendOp::ADD;
    blends[0].srcAlphaFactor = BlendFactor::ONE;
    blends[0].dstAlphaFactor = BlendFactor::ONE;
    blends[0].colorWriteMask = ColorComponent::R | ColorComponent::G |
                               ColorComponent::B | ColorComponent::A;

    blends[1].allow = true;
    blends[1].colorOp = BlendOp::ADD;
    blends[1].srcColorFactor = BlendFactor::ONE;
    blends[1].dstColorFactor = BlendFactor::ONE;
    blends[1].alphaOp = BlendOp::ADD;
    blends[1].srcAlphaFactor = BlendFactor::ONE;
    blends[1].dstAlphaFactor = BlendFactor::ONE;
    // NOTE: This could be only R, but independent blend states require a device
    // feature
    blends[1].colorWriteMask = ColorComponent::R | ColorComponent::G |
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
    pipelineSpec.depthPixelFormat =
        pass.inheritedDepth ? &spec.depthPixelFormat : nullptr;
    pipelineSpec.depthMax = 0;
    pipelineSpec.depthMin = 1;
    pipelineSpec.flipViewportY = true;
    pipelineSpec.enableDepthTest = pass.inheritedDepth ? true : false;
    pipelineSpec.enableDepthWrite = false;
    pipelineSpec.frontFaceWind = FrontFaceWind::COUNTER_CLOCKWISE;
    pipelineSpec.faceCulling = FaceCulling::BACK;
    pipelineSpec.vertexShader = vertexShader;
    pipelineSpec.fragmentShader = fragmentShader;
    pipelineSpec.pipelineLayout = pass.graphicsPipelineLayout;
    pass.graphicsPipeline = createGraphicsPipeline(context, pipelineSpec);
  }

  destroyShader(context, vertexShader);
  destroyShader(context, fragmentShader);

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
    pass.computeLayout = createDescriptorSetLayout(context, descriptorSetSpec);

    pass.computePipelineLayout =
        createPipelineLayout(context, &pass.computeLayout, 1, nullptr, 0);

    ComputePipelineSpecification pipelineSpec{};
    pipelineSpec.computeShader = computeShader;
    pipelineSpec.pipelineLayout = pass.computePipelineLayout;
    pass.computePipeline = createComputePipeline(context, pipelineSpec);
  }

  destroyShader(context, computeShader);

  {
    pass.computeSet =
        createDescriptorSet(context, spec.descriptorPool, pass.computeLayout);

    DescriptorUpdateSpecification shadingDescriptorBindings[] = {
        {0, ResourceType::SAMPLED_IMAGE, nullptr, pass.accumulator, nullptr},
        {0, ResourceType::SAMPLED_IMAGE, nullptr, pass.reveal, nullptr},
        {0, ResourceType::STORAGE_IMAGE, nullptr, pass.resolve, nullptr},
    };

    DescriptorSetUpdateSpecification descriptorSetUpdateSpec{};
    descriptorSetUpdateSpec.descriptorSet = pass.computeSet;
    descriptorSetUpdateSpec.specs = std::data(shadingDescriptorBindings);
    descriptorSetUpdateSpec.specCount = std::size(shadingDescriptorBindings);

    updateDescriptorSet(context, descriptorSetUpdateSpec);
  }

  {
    pass.shadingSet =
        createDescriptorSet(context, spec.descriptorPool, pass.shadingLayout);

    DescriptorUpdateSpecification shadingDescriptorBindings[] = {
        {0, ResourceType::UNIFORM_BUFFER, pass.shadingParamsBuffer, nullptr,
         nullptr},
        {0, ResourceType::SAMPLED_IMAGE, nullptr, spec.shadowMap, nullptr},
    };

    DescriptorSetUpdateSpecification descriptorSetUpdateSpec{};
    descriptorSetUpdateSpec.descriptorSet = pass.shadingSet;
    descriptorSetUpdateSpec.specs = std::data(shadingDescriptorBindings);
    descriptorSetUpdateSpec.specCount = std::size(shadingDescriptorBindings);

    updateDescriptorSet(context, descriptorSetUpdateSpec);
  }

  {
    RenderTarget accumulatorTarget{};
    accumulatorTarget.image = pass.accumulator;
    accumulatorTarget.targetLayout = ImageLayout::COLOR_RENDER_TARGET;
    accumulatorTarget.loadOp = LoadOp::CLEAR;
    accumulatorTarget.storeOp = StoreOp::STORE;
    accumulatorTarget.clearColor[0] = 0;
    accumulatorTarget.clearColor[1] = 0;
    accumulatorTarget.clearColor[2] = 0;
    accumulatorTarget.clearColor[3] = 1;

    RenderTarget revealTarget{};
    revealTarget.image = pass.reveal;
    revealTarget.targetLayout = ImageLayout::COLOR_RENDER_TARGET;
    revealTarget.loadOp = LoadOp::CLEAR;
    revealTarget.storeOp = StoreOp::STORE;
    revealTarget.clearColor[0] = 1;
    revealTarget.clearColor[1] = 0;
    revealTarget.clearColor[2] = 0;
    revealTarget.clearColor[3] = 0;

    RenderTarget depthTarget{};
    depthTarget.image = pass.inheritedDepth;
    depthTarget.targetLayout = ImageLayout::DEPTH_RENDER_TARGET;
    depthTarget.loadOp = LoadOp::LOAD;
    depthTarget.storeOp = StoreOp::STORE;

    RenderTarget targets[] = {accumulatorTarget, revealTarget};

    RenderPassSpecification renderPassSpec{};
    renderPassSpec.width = spec.width;
    renderPassSpec.height = spec.height;
    renderPassSpec.renderTargets = std::data(targets);
    renderPassSpec.renderTargetCount = std::size(targets);
    renderPassSpec.depthTarget = pass.inheritedDepth ? &depthTarget : nullptr;
    pass.renderPass = createRenderPass(context, renderPassSpec);
  }

  return pass;
}

void destroyWeightedOITPass(LContext context, WeightedOITPass &pass) {
  destroyDescriptorSet(context, pass.computeSet);
  destroyPipeline(context, pass.computePipeline);
  destroyPipelineLayout(context, pass.computePipelineLayout);
  destroyDescriptorSetLayout(context, pass.computeLayout);
  destroyImage(context, pass.resolve);

  destroyDescriptorSet(context, pass.shadingSet);
  destroyRenderPass(context, pass.renderPass);
  destroyPipeline(context, pass.graphicsPipeline);
  destroyPipelineLayout(context, pass.graphicsPipelineLayout);
  destroyDescriptorSetLayout(context, pass.shadingLayout);
  destroyImage(context, pass.reveal);
  destroyImage(context, pass.accumulator);
  destroyBuffer(context, pass.shadingParamsBuffer);
}

void recordWeightedOITPassDrawScene(LContext context,
                                    const WeightedOITPass &pass,
                                    const Scene &scene,
                                    DrawCommandIndexes &indexes,
                                    DrawCommand &drawCommand) {
  DeferredShadingParams params{};
  params.sunOrto = getSunOrtho(scene.sun);
  params.camPos = scene.camera.position;
  params.sunDir = scene.sun.direction;
  params.sunIntensity = scene.sun.intensity;
  params.shadowMapRes = pass.shadowMapResolution;
  params.sunCol = scene.sun.color;
  uploadBufferData(context, pass.shadingParamsBuffer, &params, 0,
                   sizeof(DeferredShadingParams));

  const ImageMetadata targetClearMeta{
      ImageLayout::UNDEFINED, QueueOwnership::GRAPHICS,
      PipelineStage::FRAGMENT_SHADER, ResourceAccess::SHADER_READ};

  const ImageMetadata renderTargetColorMeta{
      ImageLayout::COLOR_RENDER_TARGET, QueueOwnership::GRAPHICS,
      PipelineStage::RENDER_TARGET_OUTPUT, ResourceAccess::RENDER_TARGET_WRITE};

  {
    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::IMAGE_BARRIER, indexes.imageBarrierIndex, 2};
    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::BEGIN_RENDER_PASS, indexes.renderPassIndex};
    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::BIND_PIPELINE, indexes.pipelineIndex};

    drawCommand.imageBarriers[indexes.imageBarrierIndex++] = {
        pass.accumulator, targetClearMeta, renderTargetColorMeta};
    drawCommand.imageBarriers[indexes.imageBarrierIndex++] = {
        pass.reveal, targetClearMeta, renderTargetColorMeta};

    drawCommand.renderPasses[indexes.renderPassIndex] = pass.renderPass;
    drawCommand.pipelines[indexes.pipelineIndex++] = pass.graphicsPipeline;
  }

  {
    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::BIND_DESCRIPTOR_SETS,
        indexes.descriptorSetBindingIndex};
    drawCommand.descriptorSetBindings[indexes.descriptorSetBindingIndex++] = {
        scene.cameraSet, 0};

    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::BIND_DESCRIPTOR_SETS,
        indexes.descriptorSetBindingIndex};
    drawCommand.descriptorSetBindings[indexes.descriptorSetBindingIndex++] = {
        pass.shadingSet, 2};

    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::PUSH_CONSTANT, indexes.pushConstantIndex};
    drawCommand.pushConstants[indexes.pushConstantIndex].visibility =
        ShaderVisibility::FRAGMENT;
    auto camPos =
        (glm::vec3 *)drawCommand.pushConstants[indexes.pushConstantIndex].bytes;
    *camPos = scene.camera.position;
    drawCommand.pushConstants[indexes.pushConstantIndex].offset = 0;
    drawCommand.pushConstants[indexes.pushConstantIndex].size =
        sizeof(glm::vec3);
    ++indexes.pushConstantIndex;

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
      if ((material.renderFlags & alphaBlendedMatFlag) != alphaBlendedMatFlag) {
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

    std::vector<const SceneMesh *> meshRefs;
    meshRefs.reserve((scene.meshes.size()));
    for (int i = 0; i < scene.meshes.size(); ++i) {
      if (filterMesh(scene.meshes[i])) {
        meshRefs.push_back(&scene.meshes[i]);
      }
    }
    struct SortByMaterial {
      bool operator()(const SceneMesh *m1, const SceneMesh *m2) {
        return m1->material < m2->material;
      }
    };
    std::sort(meshRefs.begin(), meshRefs.end(),
              [](const SceneMesh *m1, const SceneMesh *m2) {
                return m1->material < m2->material;
              });

    uint32_t setIndex = UINT32_MAX;
    int currentMaterial = -1;
    for (int i = 0; i < meshRefs.size(); ++i) {
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
        drawCommand.operations[indexes.operationIndex++] = {
            DrawOperationType::BIND_DESCRIPTOR_SETS,
            indexes.descriptorSetBindingIndex};

        drawCommand.descriptorSetBindings[indexes.descriptorSetBindingIndex++] =
            {scene.descriptorSets[setIndex], 1};
      }
      currentMaterial = meshRefs[i]->material;

      drawCommand.operations[indexes.operationIndex++] = {
          DrawOperationType::BIND_VERTEX_BUFFER, indexes.vertexBindingIndex, 4};

      const auto &posAttr = meshRefs[i]->vertexAttributes[positionsIndex];
      drawCommand.vertexBindings[indexes.vertexBindingIndex++] = {
          scene.buffers[posAttr.buffer], 0, posAttr.offset};

      const auto &normAttr = meshRefs[i]->vertexAttributes[normalsIndex];
      drawCommand.vertexBindings[indexes.vertexBindingIndex++] = {
          scene.buffers[normAttr.buffer], 1, normAttr.offset};

      const auto &tangAttr = meshRefs[i]->vertexAttributes[tangentsIndex];
      drawCommand.vertexBindings[indexes.vertexBindingIndex++] = {
          scene.buffers[tangAttr.buffer], 2, tangAttr.offset};

      const auto &texCoordsAttr = meshRefs[i]->vertexAttributes[texCoordsIndex];
      drawCommand.vertexBindings[indexes.vertexBindingIndex++] = {
          scene.buffers[texCoordsAttr.buffer], 3, texCoordsAttr.offset};

      if (drawIndexed) {
        drawCommand.operations[indexes.operationIndex++] = {
            DrawOperationType::BIND_INDEX_BUFFER, indexes.indexBindingIndex};
        const auto &indAttr = meshRefs[i]->vertexAttributes[indexesIndex];
        drawCommand.indexBindings[indexes.indexBindingIndex++] = {
            scene.buffers[indAttr.buffer], indAttr.offset};
      }

      drawCommand.operations[indexes.operationIndex++] = {
          DrawOperationType::DRAW, indexes.drawCallIndex};
      drawCommand.drawCalls[indexes.drawCallIndex++] = {
          meshRefs[i]->elementCount, 1, drawIndexed};
    }
  }

  drawCommand.operations[indexes.operationIndex++] = {
      DrawOperationType::END_RENDER_PASS, indexes.renderPassIndex++};

  {
    const ImageMetadata shaderReadMeta{
        ImageLayout::SHADER_READ_ONLY, QueueOwnership::GRAPHICS,
        PipelineStage::FRAGMENT_SHADER, ResourceAccess::SHADER_READ};

    const ImageMetadata computeTargetMeta{
        ImageLayout::GENERAL, QueueOwnership::GRAPHICS,
        PipelineStage::COMPUTE_SHADER, ResourceAccess::SHADER_WRITE};

    uint32_t barrierCount = pass.inheritedDepth ? 4 : 3;
    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::IMAGE_BARRIER, indexes.imageBarrierIndex,
        barrierCount};

    drawCommand.imageBarriers[indexes.imageBarrierIndex++] = {
        pass.accumulator, renderTargetColorMeta, shaderReadMeta};
    drawCommand.imageBarriers[indexes.imageBarrierIndex++] = {
        pass.reveal, renderTargetColorMeta, shaderReadMeta};
    drawCommand.imageBarriers[indexes.imageBarrierIndex++] = {
        pass.resolve, targetClearMeta, computeTargetMeta};
    if (pass.inheritedDepth) {
      // TODO: I should reduce dependency here by providing the right meta
      const ImageMetadata renderTargetDepthMeta{
          ImageLayout::DEPTH_RENDER_TARGET, QueueOwnership::GRAPHICS,
          PipelineStage::RENDER_TARGET_OUTPUT,
          ResourceAccess::RENDER_TARGET_WRITE};
      drawCommand.imageBarriers[indexes.imageBarrierIndex++] = {
          pass.inheritedDepth, shaderReadMeta, renderTargetDepthMeta};
    }

    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::BIND_PIPELINE, indexes.pipelineIndex};
    drawCommand.pipelines[indexes.pipelineIndex++] = pass.computePipeline;

    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::BIND_DESCRIPTOR_SETS,
        indexes.descriptorSetBindingIndex};
    drawCommand.descriptorSetBindings[indexes.descriptorSetBindingIndex++] = {
        pass.computeSet, 0};

    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::DISPATCH, indexes.dispatchCallIndex};
    drawCommand.dispatchCalls[indexes.dispatchCallIndex].groupCountX =
        (pass.width + 15) / 16;
    drawCommand.dispatchCalls[indexes.dispatchCallIndex].groupCountY =
        (pass.height + 15) / 16;
    drawCommand.dispatchCalls[indexes.dispatchCallIndex].groupCountZ = 1;
    ++indexes.dispatchCallIndex;
  }
}
