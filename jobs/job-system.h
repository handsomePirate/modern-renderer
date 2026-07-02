#pragma once
#include "renderer.h"

#include <ring-buffer/spsc_rb.hpp>

#include <atomic>

enum class JobType {
  ASSIMP_LOAD,
  GPU_BUFFER_UPLOAD,
  GPU_IMAGE_UPLOAD,
};

struct JobRequest {
  uint64_t jobId;
  JobType type;
  void *jobData;
};

enum class JobResultCode {
  SUCCESS,

  // Job errors
  UNKNOWN_JOB_TYPE,
  MISSING_JOB_DATA,

  STAGING_BUFFER_ERROR,
  BUFFER_UPLOAD_ERROR,
  IMAGE_UPLOAD_ERROR,
  ASSIMP_SCENE_NOT_LOADED,
  SCENE_ANIM_NOT_LOADED,
  STB_IMAGE_NOT_LOADED,

  GENERIC_ERROR,
};

struct JobResult {
  uint64_t jobId;
  JobType type;
  JobResultCode code;
};

constexpr const uint32_t maxJobCount = 512;
using JobRequests =
    concurrency::spsc_ring_buffer_stack<JobRequest, maxJobCount>;
using JobResults = concurrency::spsc_ring_buffer_stack<JobResult, maxJobCount>;

void consume(LContext context, std::atomic<bool> &stop, JobRequests &jobFeed,
             JobResults &resultOutput);
