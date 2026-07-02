#include "gpu-upload-job.h"

namespace {

inline void reallocStagingBuffer(LContext context, Buffer &buffer,
                                 BufferSpecification &bufferSpec) {
  destroyBuffer(context, buffer);
  buffer = allocateBuffer(context, bufferSpec);
}

inline void makeBuffer(LContext context, Buffer &targetBuffer,
                       BufferSpecification &spec, Buffer stagingBuffer,
                       void *data, size_t offset) {
  uploadBufferData(context, stagingBuffer, data, 0, spec.size);

  if (not targetBuffer) {
    targetBuffer = allocateBuffer(context, spec);
  }
  BufferCopySpecification copySpec{};
  copySpec.size = spec.size;
  copySpec.source = stagingBuffer;
  copySpec.sourceMeta.stage = PipelineStage::TRANSFER;
  copySpec.sourceMeta.access = ResourceAccess::TRANSFER_READ;
  copySpec.sourceMeta.ownership = QueueOwnership::TRANSFER;
  copySpec.sourceOffset = 0;
  copySpec.destination = targetBuffer;
  copySpec.destinationMeta.stage = PipelineStage::TRANSFER;
  copySpec.destinationMeta.access = ResourceAccess::TRANSFER_WRITE;
  copySpec.destinationMeta.ownership = QueueOwnership::TRANSFER;
  copySpec.destinationOffset = offset;
  copyBufferData(context, copySpec);
}

} // namespace

JobResultCode processBufferGPUUploadJob(BufferGPUUploadJob *job,
                                        LContext context,
                                        BufferSpecification &stagingBufferSpec,
                                        Buffer &stagingBuffer) {
  try {
    if (stagingBufferSpec.size < job->bufferSpec.size) {
      while (stagingBufferSpec.size < job->bufferSpec.size) {
        // TODO: Cap this
        stagingBufferSpec.size *= 2;
      }
      reallocStagingBuffer(context, stagingBuffer, stagingBufferSpec);
    }
  } catch (...) {
    return JobResultCode::STAGING_BUFFER_ERROR;
  }

  try {
    makeBuffer(context, job->targetBuffer, job->bufferSpec, stagingBuffer,
               job->data, job->offset);

    return JobResultCode::SUCCESS;
  } catch (...) {
    return JobResultCode::BUFFER_UPLOAD_ERROR;
  }
}

JobResultCode processImageGPUUploadJob(ImageGPUUploadJob *job, LContext context,
                                       BufferSpecification &stagingBufferSpec,
                                       Buffer &stagingBuffer) {
  // TODO: Validate size?

  try {
    if (stagingBufferSpec.size < job->size) {
      while (stagingBufferSpec.size < job->size) {
        // TODO: Cap this
        stagingBufferSpec.size *= 2;
      }
      reallocStagingBuffer(context, stagingBuffer, stagingBufferSpec);
    }
  } catch (...) {
    return JobResultCode::STAGING_BUFFER_ERROR;
  }

  try {
    uploadBufferData(context, stagingBuffer, job->data, 0, job->size);

    ImageMetadata *outMeta = job->imageSpec.outMeta;
    ImageMetadata internalMeta;
    job->imageSpec.outMeta = &internalMeta;
    job->targetImage = createImage(context, job->imageSpec);

    ImageDataCopySpecification copySpec{};
    copySpec.image = job->targetImage;
    copySpec.imageMeta = internalMeta;
    copySpec.outImageMeta = outMeta;
    // TODO: Expose this in the job API
    copySpec.finalOwnership = QueueOwnership::GRAPHICS;
    copySpec.width = job->imageSpec.width;
    copySpec.height = job->imageSpec.height;
    copySpec.stagingBuffer = stagingBuffer;
    copySpec.bufferOffset = 0;
    copyImageData(context, copySpec);

    return JobResultCode::SUCCESS;
  } catch (...) {
    return JobResultCode::IMAGE_UPLOAD_ERROR;
  }
}
