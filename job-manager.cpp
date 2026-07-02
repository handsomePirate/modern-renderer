#include "job-manager.h"
#include "scene.h"
#include "time-keeping.h"

#include <print>

namespace {

uint64_t counter = 1;

template <typename Job, size_t Capacity>
inline bool appendJob(JobRequests &jobRequests,
                      MappedPool<Job, Capacity> &mappedPool, Job job,
                      JobType type) {
  auto jobMeta = mappedPool.pool.alloc();
  std::memset((void *)jobMeta, 0, sizeof(JobMeta<Job>));
  if (not jobMeta)
    return false;

  uint64_t jobId = counter++;

  jobMeta->jobId = jobId;
  jobMeta->job = std::move(job);

  uint64_t mapIndex = jobId % Capacity;
  if (mappedPool.map[mapIndex] != nullptr)
    return false;

  mappedPool.map[mapIndex] = jobMeta;

  JobRequest request{};
  request.jobId = jobId;
  request.type = type;
  request.jobData = &jobMeta->job;
  jobRequests.push(request);

  return true;
}

template <typename Job, size_t Capacity>
inline JobMeta<Job> removeJob(MappedPool<Job, Capacity> &mappedPool,
                              uint64_t jobId) {
  uint64_t mapIndex = jobId % Capacity;
  auto jobMeta = mappedPool.map[mapIndex];
  mappedPool.map[mapIndex] = nullptr;
  JobMeta<Job> jobMetaOut = std::move(*jobMeta);
  mappedPool.pool.free(jobMeta);
  return jobMetaOut;
}

template <typename Job, size_t Capacity>
inline Job &retrieveJob(MappedPool<Job, Capacity> &mappedPool, uint64_t jobId) {
  uint64_t mapIndex = jobId % Capacity;
  auto jobMeta = mappedPool.map[mapIndex];
  mappedPool.map[mapIndex] = nullptr;
  return jobMeta->job;
}

} // namespace

void initJobManager(JobManager &jobManager, LContext context) {
  jobManager.stopJobThread = false;
  jobManager.jobThread =
      std::thread([context, &stopJobThread = jobManager.stopJobThread,
                   &jobRequests = jobManager.jobRequests,
                   &jobResults = jobManager.jobResults]() {
        consume(context, stopJobThread, jobRequests, jobResults);
      });
}

void stopJobManager(JobManager &jobManager) {
  jobManager.stopJobThread = true;
  jobManager.jobThread.join();
}

bool appendAssimpLoadJob(JobManager &jobManager, AssimpLoadJob job) {
  return appendJob(jobManager.jobRequests, jobManager.assimpPool,
                   std::move(job), JobType::ASSIMP_LOAD);
}

bool appendBufferGPUUploadJob(JobManager &jobManager, BufferGPUUploadJob job) {
  return appendJob(jobManager.jobRequests, jobManager.bufferPool,
                   std::move(job), JobType::GPU_BUFFER_UPLOAD);
}

bool appendImageGPUUploadJob(JobManager &jobManager, ImageGPUUploadJob job) {
  return appendJob(jobManager.jobRequests, jobManager.imagePool, std::move(job),
                   JobType::GPU_IMAGE_UPLOAD);
}

namespace {

void handleSceneAddition(Scene &scene, SceneAddition &addition) {
  uint32_t bufferBaseIndex = scene.buffers.size();
  for (int i = 0; i < addition.buffers.size(); ++i) {
    scene.newBuffers.push(addition.buffers[i]);
  }
  scene.buffers.append_range(std::move(addition.buffers));

  uint32_t imageBaseIndex = scene.images.size();
  for (int i = 0; i < addition.images.size(); ++i) {
    scene.newImages.push(addition.images[i]);
  }
  scene.images.append_range(std::move(addition.images));

  // TODO: This is clearly insufficient, need more info for set update
  uint32_t descriptorSetBaseIndex = scene.descriptorSets.size();
  scene.descriptorSets.append_range(std::move(addition.descriptorSets));

  uint32_t materialBaseIndex = scene.materials.size();
  scene.materials.append_range(std::move(addition.sceneMaterials));
  for (int i = materialBaseIndex; i < scene.materials.size(); ++i) {
    for (int j = 0; j < scene.materials[i].descriptors.size(); ++j) {
      scene.materials[i].descriptors[j].descriptorSet += descriptorSetBaseIndex;
    }
  }

  uint32_t meshBaseIndex = scene.meshes.size();
  scene.meshes.append_range(std::move(addition.sceneMeshes));
  for (int i = meshBaseIndex; i < scene.meshes.size(); ++i) {
    scene.meshes[i].material += materialBaseIndex;
    for (int j = 0; j < scene.meshes[i].vertexAttributes.size(); ++j) {
      scene.meshes[i].vertexAttributes[j].buffer += bufferBaseIndex;
    }
    for (int j = 0; j < scene.meshes[i].descriptors.size(); ++j) {
      scene.meshes[i].descriptors[j].descriptorSet += descriptorSetBaseIndex;
    }
  }
}

void handleAssimpLoadGLMJob(JobManager &jobManager, Scene &scene,
                            JobResult result) {
  auto jobMeta = removeJob(jobManager.assimpPool, result.jobId);
  if (result.code != JobResultCode::SUCCESS) {
    std::println("Assimp load job {} failed", result.jobId);
    return;
  }

  handleSceneAddition(scene, jobMeta.job.sceneAddition);
}

void handleBufferGPUUploadJob(JobManager &jobManager, Scene &scene,
                              JobResult result) {
  //
}

void handleImageGPUUploadJob(JobManager &jobManager, Scene &scene,
                             JobResult result) {
  //
}

void dispatchHandleResult(JobManager &jobManager, Scene &scene,
                          JobResult result) {

  using Sig = void (*)(JobManager &, Scene &, JobResult);
  const Sig processors[] = {handleAssimpLoadGLMJob, handleBufferGPUUploadJob,
                            handleImageGPUUploadJob};

  processors[(uint32_t)result.type](jobManager, scene, result);
}

} // namespace

void updateJobManager(JobManager &jobManager, Scene &scene,
                      float timeBudgetSeconds) {
  float timeStart = getTimeSeconds();
  while (getTimeSeconds() - timeStart < timeBudgetSeconds &&
         not jobManager.jobResults.is_empty()) {
    auto resultOpt = jobManager.jobResults.pop();
    if (resultOpt) {
      auto result = *resultOpt;
      dispatchHandleResult(jobManager, scene, result);
    }
  }
}
