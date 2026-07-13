#include "job-system.h"
#include "assimp-load-job.h"
#include "gpu-upload-job.h"

#include <thread>

void consume(svet::renderer::LContext context, std::atomic<bool> &stop,
             JobRequests &jobFeed, JobResults &resultOutput,
             svet::renderer::StagingBuffer stagingBuffer) {
  while (not stop) {
    // Process all
    while (not jobFeed.is_empty()) {
      auto jobOpt = jobFeed.pop();
      if (jobOpt) {
        auto job = *jobOpt;
        auto jobData = job.jobData;
        if (jobData) {
          JobResultCode jobResultCode;
          // TODO: Could make this a runtime array dispatch in the future
          switch (job.type) {
          case JobType::ASSIMP_LOAD:
            jobResultCode = processAssimpLoadGLMJob((AssimpLoadJob *)jobData,
                                                    context, stagingBuffer);
            break;
          case JobType::GPU_BUFFER_UPLOAD:
            jobResultCode = processBufferGPUUploadJob(
                (BufferGPUUploadJob *)jobData, context, stagingBuffer);
            break;
          case JobType::GPU_IMAGE_UPLOAD:
            jobResultCode = processImageGPUUploadJob(
                (ImageGPUUploadJob *)jobData, context, stagingBuffer);
            break;
          default:
            jobResultCode = JobResultCode::UNKNOWN_JOB_TYPE;
          }
          JobResult result;
          result.jobId = job.jobId;
          result.type = job.type;
          result.code = jobResultCode;
          resultOutput.push(result);
        } else {
          JobResult result;
          result.jobId = job.jobId;
          result.type = job.type;
          result.code = JobResultCode::MISSING_JOB_DATA;
          resultOutput.push(result);
        }
      }
    }

    // Cool down
    const std::chrono::milliseconds waitPeriod(10);
    std::this_thread::sleep_for(waitPeriod);
  }
}
