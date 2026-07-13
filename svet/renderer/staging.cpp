#include "staging.h"

#include <print>

namespace svet::renderer {

StagingBuffer createStagingBuffer(LContext context, size_t size) {
  StagingBuffer stagingBuffer;
  stagingBuffer.size = size;

  MemoryPoolSpecification memoryPoolSpec{};
  memoryPoolSpec.size = size;
  memoryPoolSpec.bufferUsage = BufferUsage::TRANSFER_SRC;
  memoryPoolSpec.properties =
      MemoryProperties::HOST_VISIBLE | MemoryProperties::HOST_COHERENT;
  stagingBuffer.disposableMemoryPool =
      createMemoryPool(context, memoryPoolSpec);

  BufferSpecification bufferSpec{};
  bufferSpec.size = size;
  bufferSpec.usage = BufferUsage::TRANSFER_SRC;
  bufferSpec.memoryPool = stagingBuffer.disposableMemoryPool;
  bufferSpec.initialOwnership = QueueOwnership::TRANSFER;
  stagingBuffer.buffer = createBuffer(context, bufferSpec);

  return stagingBuffer;
}

void destroyStagingBuffer(LContext context, StagingBuffer stagingBuffer) {
  destroyBuffer(context, stagingBuffer.buffer);
  std::print("STAGING - ");
  destroyMemoryPool(context, stagingBuffer.disposableMemoryPool);
}

void stagedUploadData(LContext context, const StagedUploadSpecification &spec) {
  size_t currentOffset = 0;
  size_t remainingSize = spec.size;

  while (remainingSize > 0) {
    uint32_t currentSize = spec.stagingBuffer.size > remainingSize
                               ? remainingSize
                               : spec.stagingBuffer.size;
    remainingSize -= currentSize;
    uploadBufferData(context, spec.stagingBuffer.buffer,
                     (void *)((size_t)spec.data + currentOffset), 0,
                     currentSize);

    BufferCopySpecification copySpec{};
    copySpec.size = currentSize;
    copySpec.source = spec.stagingBuffer.buffer;
    copySpec.sourceMeta.stage = PipelineStage::TRANSFER;
    copySpec.sourceMeta.access = ResourceAccess::TRANSFER_READ;
    copySpec.sourceMeta.ownership = QueueOwnershipState::TRANSFER;
    copySpec.sourceOffset = 0;
    copySpec.destination = spec.destination;
    copySpec.destinationMeta = spec.destinationMeta;
    copySpec.outDestinationMeta =
        remainingSize > 0 ? nullptr : spec.outDestinationMeta;
    copySpec.destinationOffset = spec.offset + currentOffset;
    copySpec.desiredSrcOwnership = QueueOwnership::TRANSFER;
    copySpec.desiredDstOwnership =
        remainingSize > 0 ? QueueOwnership::TRANSFER : spec.desiredOwnership;
    copyBufferData(context, copySpec);
    currentOffset += currentSize;
  }
}

void stagedUploadImageData(LContext context,
                           const StagedUploadImageSpecification &spec) {
  size_t currentRow = 0;
  size_t remainingRows = spec.height;
  uint32_t rowSize = spec.pixelSize * spec.width;

  uint32_t rowsInSingleUpload = spec.stagingBuffer.size / rowSize;

  if (rowsInSingleUpload == 0) {
    std::println("[Vulkan] Staging buffer too small to upload this image, must "
                 "be at least row size (={})",
                 rowSize);
    return;
  } else {
    while (remainingRows > 0) {
      uint32_t currentRows = rowsInSingleUpload > remainingRows
                                 ? remainingRows
                                 : rowsInSingleUpload;
      remainingRows -= currentRows;
      uploadBufferData(context, spec.stagingBuffer.buffer,
                       (void *)((size_t)spec.data + currentRow * rowSize), 0,
                       currentRows * rowSize);

      ImageDataCopySpecification copySpec{};
      copySpec.width = spec.width;
      copySpec.height = currentRows;
      copySpec.offsetX = 0;
      copySpec.offsetY = currentRow;
      copySpec.bufferOffset = 0;
      copySpec.stagingBuffer = spec.stagingBuffer.buffer;
      copySpec.stagingBufferMeta.stage = PipelineStage::TRANSFER;
      copySpec.stagingBufferMeta.access = ResourceAccess::TRANSFER_READ;
      copySpec.stagingBufferMeta.ownership = QueueOwnershipState::TRANSFER;
      copySpec.image = spec.destination;
      copySpec.imageMeta = spec.destinationMeta;
      copySpec.outImageMeta =
          remainingRows > 0 ? nullptr : spec.outDestinationMeta;
      copySpec.desiredStagingBufferOwnership = QueueOwnership::TRANSFER;
      copySpec.desiredImageLayout =
          remainingRows > 0 ? ImageLayout::TRANSFER_DST : spec.desiredLayout;
      copySpec.desiredImageOwnership =
          remainingRows > 0 ? QueueOwnership::TRANSFER : spec.desiredOwnership;
      copyImageData(context, copySpec);
      currentRow += currentRows;
    }
  }
}

} // namespace svet::renderer
