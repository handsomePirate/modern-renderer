#include "gpu-upload-job.h"

namespace {

inline void makeBuffer(svet::renderer::LContext context,
                       svet::renderer::Buffer &targetBuffer,
                       svet::renderer::BufferSpecification &spec,
                       svet::renderer::StagingBuffer stagingBuffer, void *data,
                       size_t offset) {
  using namespace svet::renderer;

  if (not targetBuffer) {
    targetBuffer = createBuffer(context, spec);
  }

  StagedUploadSpecification uploadSpec{};
  uploadSpec.stagingBuffer = stagingBuffer;
  uploadSpec.destination = targetBuffer;
  uploadSpec.offset = offset;
  uploadSpec.size = spec.size;
  uploadSpec.data = data;
  stagedUploadData(context, uploadSpec);
}

} // namespace

JobResultCode
processBufferGPUUploadJob(BufferGPUUploadJob *job,
                          svet::renderer::LContext context,
                          svet::renderer::StagingBuffer stagingBuffer) {

  try {
    makeBuffer(context, job->targetBuffer, job->bufferSpec, stagingBuffer,
               job->data, job->offset);

    return JobResultCode::SUCCESS;
  } catch (...) {
    return JobResultCode::BUFFER_UPLOAD_ERROR;
  }
}

JobResultCode
processImageGPUUploadJob(ImageGPUUploadJob *job,
                         svet::renderer::LContext context,
                         svet::renderer::StagingBuffer stagingBuffer) {
  using namespace svet::renderer;

  try {
    uploadBufferData(context, stagingBuffer.buffer, job->data, 0, job->size);

    ImageMetadata *outMeta = job->imageSpec.outMeta;
    ImageMetadata internalMeta;
    job->imageSpec.outMeta = &internalMeta;
    job->targetImage = createImage(context, job->imageSpec);

    ImageDataCopySpecification copySpec{};
    copySpec.image = job->targetImage;
    copySpec.imageMeta = internalMeta;
    copySpec.outImageMeta = outMeta;
    // TODO: Expose this in the job API
    copySpec.desiredImageOwnership = QueueOwnership::GRAPHICS;
    copySpec.width = job->imageSpec.width;
    copySpec.height = job->imageSpec.height;
    copySpec.stagingBuffer = stagingBuffer.buffer;
    copySpec.bufferOffset = 0;
    copyImageData(context, copySpec);

    return JobResultCode::SUCCESS;
  } catch (...) {
    return JobResultCode::IMAGE_UPLOAD_ERROR;
  }
}
