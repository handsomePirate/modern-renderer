#include "scene.h"
#include "image-gen.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/gtc/matrix_transform.hpp>

Scene createScene(LContext context, const SceneSpecification &spec) {
  Scene scene;
  scene.defaultAlbedo = genSinglePixel(context, 255, 0, 255);
  scene.defaultHeight = genSinglePixel(context, 127, 127, 127);

  scene.descriptorPool = spec.descriptorPool;
  SamplerSpecification samplerSpec{};
  samplerSpec.addressingU = SampleAddressing::REPEAT;
  samplerSpec.addressingV = SampleAddressing::REPEAT;
  samplerSpec.minFilter = SampleFilter::NEAREST;
  samplerSpec.magFilter = SampleFilter::NEAREST;
  scene.sampler = createSampler(context, samplerSpec);

  {
    BufferSpecification cameraBufferSpec{};
    cameraBufferSpec.size = sizeof(glm::mat4);
    cameraBufferSpec.source = BufferSource::CPU;
    cameraBufferSpec.consumer = BufferConsumer::GPU;
    cameraBufferSpec.frequency = BufferFrequency::DYNAMIC;
    cameraBufferSpec.usage = BufferUsage::UNIFORM;
    cameraBufferSpec.initialOwnership = QueueOwnership::GRAPHICS;
    scene.cameraBuffer = allocateBuffer(context, cameraBufferSpec);
  }

  {
    ResourceType matType = ResourceType::UNIFORM_BUFFER;
    ShaderVisibility matVis = ShaderVisibility::VERTEX;
    DescriptorSetLayoutSpecification camSetLayoutSpec{};
    camSetLayoutSpec.types = &matType;
    camSetLayoutSpec.visibilities = &matVis;
    camSetLayoutSpec.typeCount = 1;
    scene.cameraSetLayout =
        createDescriptorSetLayout(context, camSetLayoutSpec);

    DescriptorUpdateSpecification descriptorBindings[] = {
        {0, ResourceType::UNIFORM_BUFFER, scene.cameraBuffer, nullptr, nullptr},
    };
    scene.cameraSet = createDescriptorSet(context, spec.descriptorPool,
                                          scene.cameraSetLayout);

    DescriptorSetUpdateSpecification descriptorSetUpdateSpec{};
    descriptorSetUpdateSpec.descriptorSet = scene.cameraSet;
    descriptorSetUpdateSpec.specs = std::data(descriptorBindings);
    descriptorSetUpdateSpec.specCount = std::size(descriptorBindings);

    updateDescriptorSet(context, descriptorSetUpdateSpec);
  }

  return scene;
}

void recordSceneLoadCommands(LContext context, Scene &scene,
                             DrawCommandIndexes &indexes,
                             DrawCommand &drawCommand) {
  const ImageMetadata loadedTextureImageMeta{
      ImageLayout::TRANSFER_DST, QueueOwnership::TRANSFER_RELEASED_TO_GRAPHICS,
      PipelineStage::TRANSFER, ResourceAccess::TRANSFER_WRITE};

  const ImageMetadata shaderReadImageMeta{
      ImageLayout::SHADER_READ_ONLY, QueueOwnership::GRAPHICS,
      PipelineStage::FRAGMENT_SHADER, ResourceAccess::SHADER_READ};

  const BufferMetadata bufferCopyBufferMeta{
      QueueOwnership::TRANSFER_RELEASED_TO_GRAPHICS, PipelineStage::TRANSFER,
      ResourceAccess::TRANSFER_WRITE};

  PipelineStage s;
  const BufferMetadata vertexAttribBufferMeta{
      QueueOwnership::GRAPHICS, PipelineStage::VERTEX_ATTRIBUTE_INPUT,
      ResourceAccess::VERTEX_ATTRIBUTE_READ};

  if (not scene.newBuffers.is_empty()) {
    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::BUFFER_BARRIER, indexes.bufferBarrierIndex,
        (uint32_t)scene.newBuffers.size()};
    while (not scene.newBuffers.is_empty()) {
      auto newBuffer = scene.newBuffers.pop().value();
      // TODO: Different types of buffers could be here
      drawCommand.bufferBarriers[indexes.bufferBarrierIndex++] = {
          newBuffer, bufferCopyBufferMeta, vertexAttribBufferMeta};
    }
  }

  if (not scene.newImages.is_empty()) {
    drawCommand.operations[indexes.operationIndex++] = {
        DrawOperationType::IMAGE_BARRIER, indexes.imageBarrierIndex,
        (uint32_t)scene.newImages.size()};
    while (not scene.newImages.is_empty()) {
      auto newImage = scene.newImages.pop().value();
      drawCommand.imageBarriers[indexes.imageBarrierIndex++] = {
          newImage, loadedTextureImageMeta, shaderReadImageMeta};
    }
  }
}

void destroyScene(LContext context, Scene &scene) {
  for (int i = 0; i < scene.descriptorSets.size(); ++i) {
    destroyDescriptorSet(context, scene.descriptorSets[i]);
  }

  destroyDescriptorSetLayout(context, scene.cameraSetLayout);

  for (int i = 0; i < scene.images.size(); ++i) {
    destroyImage(context, scene.images[i]);
  }

  for (int i = 0; i < scene.buffers.size(); ++i) {
    destroyBuffer(context, scene.buffers[i]);
  }

  for (int i = 0; i < scene.texturedMeshSets.size(); ++i) {
    destroyDescriptorSet(context, scene.texturedMeshSets[i]);
  }

  destroySampler(context, scene.sampler);

  destroyImage(context, scene.defaultAlbedo);
  destroyImage(context, scene.defaultHeight);

  destroyDescriptorSet(context, scene.cameraSet);
  destroyBuffer(context, scene.cameraBuffer);
}

glm::mat4 getVP(const SceneCamera &camera) {
  glm::vec3 cameraForward = camera.orientation * cameraBaseForward;
  glm::vec3 cameraUp = camera.orientation * cameraBaseUp;

  glm::mat4 view =
      glm::lookAt(camera.position, camera.position + cameraForward, cameraUp);

  glm::mat4 projection = glm::perspective(
      glm::radians(camera.fov), camera.aspect, camera.nearZ, camera.farZ);

  return projection * view;
}

glm::mat4 getSunView(const SceneSun &sun) {
  glm::vec3 sceneCenter = glm::vec3(0);
  glm::vec3 sunPos =
      sceneCenter + sun.direction * 2200.0f; // Move back along direction
  glm::vec3 sunUp =
      glm::abs(glm::dot(sun.direction, glm::vec3(0, 0, 1))) < 0.99f
          ? glm::vec3(0, 0, 1)
          : glm::vec3(1, 0, 0);
  glm::mat4 sunView = glm::lookAt(sunPos, sceneCenter, sunUp);
  return sunView;
}

glm::mat4 getSunOrtho(const SceneSun &sun) {
  glm::mat4 sunView = getSunView(sun);
  glm::vec3 minBounds, maxBounds;
  minBounds.x = -2500.f;
  minBounds.y = -2500.f;
  minBounds.z = 0.f;
  maxBounds.x = 2500.f;
  maxBounds.y = 2500.f;
  maxBounds.z = 2500.f;

  glm::mat4 sunProj = glm::ortho(minBounds.x, maxBounds.x, minBounds.y,
                                 maxBounds.y, minBounds.z, maxBounds.z);

  return sunProj * sunView;
}
