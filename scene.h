#pragma once
#include "renderer.h"
#include "types.h"

#include <ext/ring-buffer/st_rb.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <vector>

static constexpr const glm::vec3 cameraBaseForward = {1, 0, 0};
static constexpr const glm::vec3 cameraBaseUp = {0, 0, 1};
static constexpr const glm::vec3 cameraBaseRight = {0, -1, 0};

struct SceneVertexAttribute {
  uint32_t signature;
  uint32_t buffer;
  uint32_t offset;
  uint32_t size;
};

struct SceneDescriptor {
  uint32_t signature;
  uint32_t asset;
  uint32_t descriptorSet;
};

struct SceneBuffer {
  uint32_t buffer;
  uint32_t offset;
  uint32_t size;
};

struct SceneImage {
  uint32_t image;
  uint32_t width;
  uint32_t height;
};

struct SceneAnimation {
  uint32_t animation;
  uint32_t transformBuffer;
};

struct SceneMesh {
  uint32_t renderFlags;
  // uint32_t object;
  uint32_t material;
  std::vector<SceneVertexAttribute> vertexAttributes;
  std::vector<SceneDescriptor> descriptors;
  uint32_t elementCount;
  uint32_t instanceCount;
};

struct SceneMaterial {
  uint32_t renderFlags;
  std::vector<SceneDescriptor> descriptors;
};

struct SceneAddition {
  // TODO: supply buffers to allocate things in

  std::vector<Buffer> buffers;
  std::vector<Image> images;
  std::vector<DescriptorSet> descriptorSets;
  std::vector<SceneMesh> sceneMeshes;
  std::vector<SceneMaterial> sceneMaterials;
  std::vector<SceneAnimation> sceneAnimations;
};

static constexpr const uint32_t positionsVASignature = 0x00000001;
static constexpr const uint32_t normalsVASignature = 0x00000002;
static constexpr const uint32_t tangentsVASignature = 0x00000003;
static constexpr const uint32_t texCoordsVASignature = 0x00000004;
static constexpr const uint32_t indexesVASignature = 0x00000005;
static constexpr const uint32_t transformsVASignature = 0x00000006;
static constexpr const uint32_t boneIndexesVASignature = 0x00000007;
static constexpr const uint32_t boneWeightsVASignature = 0x00000008;

static constexpr const uint32_t cameraVPDASignature = 0x00000001;
static constexpr const uint32_t albedoTexDASignature = 0x00000002;
static constexpr const uint32_t normalTexDASignature = 0x00000003;
static constexpr const uint32_t metalroughTexDASignature = 0x00000004;

static constexpr const uint32_t alphaBlendedMatFlag = 0x00000001;
static constexpr const uint32_t binaryTransparencyMatFlag = 0x00000002;

struct SceneCamera {
  glm::vec3 position;
  glm::quat orientation;
  float fov;
  float aspect;
  float nearZ;
  float farZ;
};
struct SceneSun {
  glm::vec3 direction;
  float intensity;
  glm::vec3 color;
};

struct Scene {
  SceneCamera camera;
  DescriptorSet cameraSet;
  Buffer cameraBuffer;
  SceneSun sun;

  // Assets to be processed before they can be used
  ring_buffer::st_ring_buffer<Buffer, 512> newBuffers;
  ring_buffer::st_ring_buffer<Image, 512> newImages;

  // Assets ready for use
  std::vector<Buffer> buffers;
  std::vector<Image> images;
  std::vector<SceneMesh> meshes;
  std::vector<SceneMaterial> materials;
  std::vector<DescriptorSet> descriptorSets;

  Image defaultAlbedo;
  Image defaultHeight;

  DescriptorPool descriptorPool;
  DescriptorSetLayout cameraSetLayout;
  Sampler sampler;
  std::vector<DescriptorSet> texturedMeshSets;
};

struct SceneSpecification {
  DescriptorPool descriptorPool;
  DescriptorSetLayout cameraSetLayout;
};
Scene createScene(LContext context, const SceneSpecification &spec);
void recordSceneLoadCommands(LContext context, Scene &scene,
                             DrawCommandIndexes &indexes,
                             DrawCommand &drawCommand);
void destroyScene(LContext context, Scene &scene);
glm::mat4 getVP(const SceneCamera &camera);
glm::mat4 getSunView(const SceneSun &sun);
glm::mat4 getSunOrtho(const SceneSun &sun);
