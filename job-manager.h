#pragma once
#include "jobs/assimp-load-job.h"
#include "jobs/gpu-upload-job.h"

#include <pool-allocator/pool_allocator.hpp>
#include <ring-buffer/spsc_rb.hpp>
#include <ring-buffer/st_rb.hpp>

#include <thread>

struct Scene;

constexpr const inline uint32_t maxAssimpLoadJobs = 16;
constexpr const inline uint32_t maxBufferGPUUploadJobs = 64;
constexpr const inline uint32_t maxImageGPUUploadJobs = 64;

template <typename Job> struct JobMeta {
  uint64_t jobId;
  Job job;
};

template <typename Job, size_t Capacity> struct MappedPool {
  allocation::pool_allocator_heap<JobMeta<Job>, Capacity> pool;
  std::array<JobMeta<Job> *, Capacity> map;
};

struct JobManager {
  MappedPool<AssimpLoadJob, maxAssimpLoadJobs> assimpPool;
  MappedPool<BufferGPUUploadJob, maxBufferGPUUploadJobs> bufferPool;
  MappedPool<ImageGPUUploadJob, maxImageGPUUploadJobs> imagePool;

  JobRequests jobRequests;
  JobResults jobResults;
  std::atomic<bool> stopJobThread;
  std::thread jobThread;
};

void initJobManager(JobManager &jobManager, LContext context);
void stopJobManager(JobManager &jobManager);
bool appendAssimpLoadJob(JobManager &jobManager, AssimpLoadJob job);
bool appendBufferGPUUploadJob(JobManager &jobManager, BufferGPUUploadJob job);
bool appendImageGPUUploadJob(JobManager &jobManager, ImageGPUUploadJob job);
void updateJobManager(JobManager &jobManager, Scene &scene,
                      float timeBudgetSeconds);
