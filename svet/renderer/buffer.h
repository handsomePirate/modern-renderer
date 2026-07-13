#pragma once
#include "context.h"
#include "enums.h"
#include "memory.h"

#include <cstddef>

namespace svet::renderer {

using Buffer = struct BufferT *;

struct BufferMetadata {
  QueueOwnershipState ownership;
  PipelineStage stage;
  ResourceAccess access;
};
struct BufferSpecification {
  size_t size;
  BufferUsage usage;
  BufferMetadata *outMeta;
  QueueOwnership initialOwnership;
  MemoryPool memoryPool;
};
struct BufferCopySpecification {
  Buffer source;
  BufferMetadata sourceMeta;
  BufferMetadata *outSourceMeta;
  Buffer destination;
  BufferMetadata destinationMeta;
  BufferMetadata *outDestinationMeta;
  uint32_t sourceOffset;
  uint32_t destinationOffset;
  uint32_t size;
  QueueOwnership desiredSrcOwnership;
  QueueOwnership desiredDstOwnership;
};
Buffer createBuffer(LContext context, const BufferSpecification &spec);
void uploadBufferData(LContext context, Buffer buffer, const void *data,
                      size_t offset, size_t size);
void *mapBuffer(LContext context, Buffer buffer, size_t offset, size_t size);
void flushBuffer(LContext context, Buffer buffer, size_t offset, size_t size);
void unmapBuffer(LContext context, Buffer buffer);
void copyBufferData(LContext context, const BufferCopySpecification &spec);
void destroyBuffer(LContext context, Buffer buffer);
void transitionBuffer(LContext context, Buffer buffer, BufferMetadata meta,
                      BufferMetadata outMeta);

} // namespace svet::renderer
