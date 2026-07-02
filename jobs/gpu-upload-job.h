#pragma once
#include "job-system.h"
#include "renderer.h"

#include <cstddef>

struct BufferGPUUploadJob {
  void *data;
  size_t offset;

  BufferSpecification bufferSpec;
  Buffer targetBuffer;
};

JobResultCode processBufferGPUUploadJob(BufferGPUUploadJob *job,
                                        LContext context,
                                        BufferSpecification &stagingBufferSpec,
                                        Buffer &stagingBuffer);

struct ImageGPUUploadJob {
  uint8_t *data;
  size_t size;

  ImageSpecification imageSpec;
  Image targetImage;
  ImageMetadata targetImageMeta;
};

JobResultCode processImageGPUUploadJob(ImageGPUUploadJob *job, LContext context,
                                       BufferSpecification &stagingBufferSpec,
                                       Buffer &stagingBuffer);
