#pragma once
#include "job-system.h"
#include "renderer.h"
#include "scene.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

/*
struct GLMBoneAnimation {
  uint32_t boneIndex;
  float duration;
  glm::vec3 *posKeys;
  float *posTimes;
  uint32_t posKeyCount;
  glm::quat *rotKeys;
  float *rotTimes;
  uint32_t rotKeyCount;
  glm::vec3 *scaKeys;
  float *scaTimes;
  uint32_t scaKeyCount;
};

struct GLMAnimation {
  GLMBoneAnimation *boneAnimations;
  uint32_t boneAnimationCount;
  uint32_t boneCount;
};

struct GLMMesh {
  glm::vec3 *positions;
  glm::vec3 *normals;
  glm::vec3 *tangents;
  glm::vec2 *texCoords[8];
  glm::vec4 *boneWeights;
  glm::uvec4 *boneIndexes;
  uint32_t texCoordCount;
  uint32_t *indexes;
  uint32_t vertexCount;
  uint32_t indexCount;
  uint32_t materialIndex;

  glm::mat4 *invBindPoses;
  // TODO: Probably can get away with uint8_t - this olaso influences
  // boneIndexes, which can be packed into a single uint32_t
  uint32_t *boneParents;
  uint32_t boneCount;
};
*/

struct AssimpLoadJob {
  const char *file;
  bool rotateZUp;
  float scale;
  glm::vec3 move;
  DescriptorPool descriptorPool;
  Sampler sampler;
  // TODO: supply buffers to allocate things in

  SceneAddition sceneAddition;
};

JobResultCode processAssimpLoadGLMJob(AssimpLoadJob *job, LContext context,
                                      BufferSpecification &stagingBufferSpec,
                                      Buffer &stagingBuffer);
