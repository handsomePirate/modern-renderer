#pragma once
#include "buffer.h"
#include "context.h"
#include "enums.h"

namespace svet::renderer {

using Image = struct ImageT *;

struct ImageMetadata {
  ImageLayout layout;
  QueueOwnershipState ownership;
  PipelineStage stage;
  ResourceAccess access;
};
struct ImageSpecification {
  uint32_t width;
  uint32_t height;
  PixelFormat pixelFormat;
  ImageUsage usage;
  ImageTiling tiling;
  ImageLayout initialLayout;
  ImageMetadata *outMeta;
  QueueOwnership initialOwnership;
  MemoryPool memoryPool;
};
struct ImageDataCopySpecification {
  Image image;
  ImageMetadata imageMeta;
  ImageMetadata *outImageMeta;
  Buffer stagingBuffer;
  BufferMetadata stagingBufferMeta;
  BufferMetadata *outStagingBufferMeta;
  size_t bufferOffset;
  uint32_t offsetX;
  uint32_t offsetY;
  uint32_t width;
  uint32_t height;
  ImageLayout desiredImageLayout;
  QueueOwnership desiredImageOwnership;
  QueueOwnership desiredStagingBufferOwnership;
};
struct ImageCopySpecification {
  Image source;
  ImageMetadata sourceMeta;
  ImageMetadata *outSourceMeta;
  Image destination;
  ImageMetadata destinationMeta;
  ImageMetadata *outDestinationMeta;
  uint32_t srcX;
  uint32_t srcY;
  uint32_t dstX;
  uint32_t dstY;
  uint32_t width;
  uint32_t height;
  ImageLayout desiredSrcLayout;
  QueueOwnership desiredSrcOwnership;
  ImageLayout desiredDstLayout;
  QueueOwnership desiredDstOwnership;
};
struct ImageBlitSpecification {
  Image source;
  ImageMetadata sourceMeta;
  ImageMetadata *outSourceMeta;
  Image destination;
  ImageMetadata destinationMeta;
  ImageMetadata *outDestinationMeta;
  SampleFilter filter;
  ImageLayout desiredSrcLayout;
  QueueOwnership desiredSrcOwnership;
  ImageLayout desiredDstLayout;
  QueueOwnership desiredDstOwnership;
};
Image createImage(LContext context, const ImageSpecification &spec);
void destroyImage(LContext context, Image image);
void copyImageData(LContext context, const ImageDataCopySpecification &spec);
void copyImage(LContext context, const ImageCopySpecification &spec);
void blitImage(LContext context, const ImageBlitSpecification &spec);
void transitionImage(LContext context, Image image, ImageMetadata meta,
                     ImageMetadata outMeta);

} // namespace svet::renderer
