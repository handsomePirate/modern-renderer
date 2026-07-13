#pragma once
#include "job-system.h"
#include "svet/renderer/buffer.h"
#include "svet/renderer/context.h"
#include "svet/renderer/image.h"
#include "svet/renderer/staging.h"

#include <cstddef>

struct BufferGPUUploadJob {
  void *data;
  size_t offset;

  svet::renderer::BufferSpecification bufferSpec;
  svet::renderer::Buffer targetBuffer;
};

JobResultCode
processBufferGPUUploadJob(BufferGPUUploadJob *job,
                          svet::renderer::LContext context,
                          svet::renderer::StagingBuffer stagingBuffer);

struct ImageGPUUploadJob {
  uint8_t *data;
  size_t size;

  svet::renderer::ImageSpecification imageSpec;
  svet::renderer::Image targetImage;
  svet::renderer::ImageMetadata targetImageMeta;
};

JobResultCode
processImageGPUUploadJob(ImageGPUUploadJob *job,
                         svet::renderer::LContext context,
                         svet::renderer::StagingBuffer stagingBuffer);
