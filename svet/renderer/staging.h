#pragma once
#include "svet/renderer/buffer.h"
#include "svet/renderer/context.h"
#include "svet/renderer/image.h"

namespace svet::renderer {

struct StagingBuffer {
  MemoryPool disposableMemoryPool;
  Buffer buffer;
  size_t size;
};

StagingBuffer createStagingBuffer(LContext context, size_t size);
void destroyStagingBuffer(LContext context, StagingBuffer stagingBuffer);
struct StagedUploadSpecification {
  StagingBuffer stagingBuffer;
  Buffer destination;
  BufferMetadata destinationMeta;
  BufferMetadata *outDestinationMeta;
  QueueOwnership desiredOwnership;
  size_t offset;
  size_t size;
  void *data;
};
void stagedUploadData(LContext context, const StagedUploadSpecification &spec);
struct StagedUploadImageSpecification {
  StagingBuffer stagingBuffer;
  Image destination;
  ImageMetadata destinationMeta;
  ImageMetadata *outDestinationMeta;
  ImageLayout desiredLayout;
  QueueOwnership desiredOwnership;
  void *data;
  uint32_t width;
  uint32_t height;
  uint32_t pixelSize;
};
void stagedUploadImageData(LContext context,
                           const StagedUploadImageSpecification &spec);

} // namespace svet::renderer
