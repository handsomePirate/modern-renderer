#pragma once
#include "job-system.h"
#include "scene.h"
#include "svet/renderer/context.h"
#include "svet/renderer/descriptor.h"
#include "svet/renderer/sampler.h"
#include "svet/renderer/staging.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct AssimpLoadJob {
  const char *file;
  bool rotateZUp;
  float scale;
  glm::vec3 move;
  svet::renderer::MemoryPool bufferPool;
  svet::renderer::MemoryPool imagePool;
  svet::renderer::DescriptorPool descriptorPool;
  svet::renderer::Sampler sampler;
  // TODO: supply buffers to allocate things in

  SceneAddition sceneAddition;
};

JobResultCode
processAssimpLoadGLMJob(AssimpLoadJob *job, svet::renderer::LContext context,
                        svet::renderer::StagingBuffer stagingBuffer);
