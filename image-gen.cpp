#include "image-gen.h"

Image genCheckerPattern(LContext context, uint32_t width, uint32_t height) {
  uint32_t imageWidth = width;
  uint32_t imageHeight = height;
  uint32_t pixelSize = sizeof(uint8_t) * 4;
  uint32_t imageDataSize = imageWidth * imageHeight * pixelSize;
  uint8_t *imageData = new uint8_t[imageDataSize];
  for (int y = 0; y < imageHeight; ++y) {
    for (int x = 0; x < imageWidth; ++x) {
      auto baseIndex = (y * imageWidth + x) * 4;
      if ((x >= imageWidth / 2) != (y >= imageHeight / 2)) {
        imageData[baseIndex + 0] = 255;
        imageData[baseIndex + 1] = 0;
        imageData[baseIndex + 2] = 0;
        imageData[baseIndex + 3] = 255;
      } else {
        imageData[baseIndex + 0] = 0;
        imageData[baseIndex + 1] = 255;
        imageData[baseIndex + 2] = 0;
        imageData[baseIndex + 3] = 255;
      }
    }
  }
  BufferSpecification stagingBufferSpec{};
  stagingBufferSpec.size = imageDataSize;
  stagingBufferSpec.source = BufferSource::CPU;
  stagingBufferSpec.consumer = BufferConsumer::GPU;
  stagingBufferSpec.frequency = BufferFrequency::STREAM;
  stagingBufferSpec.usage = BufferUsage::TRANSFER_SRC;
  stagingBufferSpec.initialOwnership = QueueOwnership::TRANSFER;
  Buffer stagingBuffer = allocateBuffer(context, stagingBufferSpec);
  uploadBufferData(context, stagingBuffer, imageData, 0, imageDataSize);
  delete[] imageData;

  ImageMetadata meta;
  ImageSpecification imageSpec{};
  imageSpec.outMeta = &meta;
  imageSpec.width = imageWidth;
  imageSpec.height = imageHeight;
  imageSpec.pixelFormat = PixelFormat::UNORM8_RGBA;
  imageSpec.usage = ImageUsage::TRANSFER_DST | ImageUsage::SAMPLED;
  imageSpec.tiling = ImageTiling::OPTIMAL;
  Image image = createImage(context, imageSpec);
  ImageDataCopySpecification copySpec{};
  copySpec.image = image;
  copySpec.imageMeta = meta;
  copySpec.outImageMeta = &meta;
  copySpec.stagingBuffer = stagingBuffer;
  copySpec.bufferOffset = 0;
  copySpec.width = imageWidth;
  copySpec.height = imageHeight;
  copySpec.finalOwnership = QueueOwnership::GRAPHICS;
  copyImageData(context, copySpec);
  graphicsAcquireImage(context, image, meta, nullptr);
  destroyBuffer(context, stagingBuffer);

  return image;
}

Image genGradient(LContext context, uint32_t width, uint32_t height) {
  uint8_t fourPixels[] = {
      0,   0,   0,   255, // pixel 0, 0
      255, 255, 255, 255, // pixel 1, 0
      0,   0,   0,   255, // pixel 1, 1
      255, 255, 255, 255, // pixel 0, 1
  };

  uint32_t imageWidth = 2;
  uint32_t imageHeight = 2;
  uint32_t finalImageWidth = 400;
  uint32_t finalImageHeight = 400;
  BufferSpecification stagingBufferSpec{};
  stagingBufferSpec.size = sizeof(fourPixels);
  stagingBufferSpec.source = BufferSource::CPU;
  stagingBufferSpec.consumer = BufferConsumer::GPU;
  stagingBufferSpec.frequency = BufferFrequency::STREAM;
  stagingBufferSpec.usage = BufferUsage::TRANSFER_SRC;
  stagingBufferSpec.initialOwnership = QueueOwnership::TRANSFER;
  Buffer stagingBuffer = allocateBuffer(context, stagingBufferSpec);
  uploadBufferData(context, stagingBuffer, fourPixels, 0, sizeof(fourPixels));

  ImageMetadata intermediateMeta;
  ImageSpecification imageSpec{};
  imageSpec.outMeta = &intermediateMeta;
  imageSpec.width = imageWidth;
  imageSpec.height = imageHeight;
  imageSpec.pixelFormat = PixelFormat::UNORM8_RGBA;
  imageSpec.usage =
      ImageUsage::TRANSFER_SRC | ImageUsage::TRANSFER_DST | ImageUsage::SAMPLED;
  imageSpec.tiling = ImageTiling::OPTIMAL;
  Image intermediateImage = createImage(context, imageSpec);
  ImageDataCopySpecification copySpec{};
  copySpec.image = intermediateImage;
  copySpec.imageMeta = intermediateMeta;
  copySpec.outImageMeta = &intermediateMeta;
  copySpec.stagingBuffer = stagingBuffer;
  copySpec.bufferOffset = 0;
  copySpec.width = imageWidth;
  copySpec.height = imageHeight;
  copySpec.finalOwnership = QueueOwnership::GRAPHICS;
  copyImageData(context, copySpec);
  destroyBuffer(context, stagingBuffer);

  ImageMetadata meta;
  imageSpec.outMeta = &meta;
  imageSpec.width = finalImageWidth;
  imageSpec.height = finalImageHeight;
  imageSpec.usage = ImageUsage::TRANSFER_DST | ImageUsage::SAMPLED;
  Image image = createImage(context, imageSpec);

  ImageBlitSpecification blitSpec{};
  blitSpec.source = intermediateImage;
  blitSpec.sourceMeta = intermediateMeta;
  blitSpec.destination = image;
  blitSpec.destinationMeta = meta;
  blitSpec.outDestinationMeta = &meta;
  blitSpec.filter = SampleFilter::LINEAR;
  blitImage(context, blitSpec);
  graphicsAcquireImage(context, image, meta, nullptr);

  destroyImage(context, intermediateImage);

  return image;
}

Image genDefaultNormal(LContext context) {
  return genSinglePixel(context, 127, 127, 255);
}

Image genSinglePixel(LContext context, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t pixel[] = {r, g, b, 255};

  BufferMetadata stagingBufferMeta;
  BufferSpecification stagingBufferSpec{};
  stagingBufferSpec.size = sizeof(pixel);
  stagingBufferSpec.source = BufferSource::CPU;
  stagingBufferSpec.consumer = BufferConsumer::GPU;
  stagingBufferSpec.frequency = BufferFrequency::STREAM;
  stagingBufferSpec.usage = BufferUsage::TRANSFER_SRC;
  stagingBufferSpec.outMeta = &stagingBufferMeta;
  stagingBufferSpec.initialOwnership = QueueOwnership::TRANSFER;
  Buffer stagingBuffer = allocateBuffer(context, stagingBufferSpec);
  uploadBufferData(context, stagingBuffer, pixel, 0, sizeof(pixel));

  ImageMetadata meta;
  ImageSpecification imageSpec{};
  imageSpec.outMeta = &meta;
  imageSpec.width = 1;
  imageSpec.height = 1;
  imageSpec.pixelFormat = PixelFormat::UNORM8_RGBA;
  imageSpec.usage = ImageUsage::TRANSFER_DST | ImageUsage::SAMPLED;
  imageSpec.tiling = ImageTiling::OPTIMAL;
  Image image = createImage(context, imageSpec);
  ImageDataCopySpecification copySpec{};
  copySpec.image = image;
  copySpec.imageMeta = meta;
  copySpec.outImageMeta = &meta;
  copySpec.stagingBuffer = stagingBuffer;
  copySpec.stagingBufferMeta = stagingBufferMeta;
  copySpec.bufferOffset = 0;
  copySpec.width = 1;
  copySpec.height = 1;
  copySpec.finalOwnership = QueueOwnership::GRAPHICS;
  copyImageData(context, copySpec);
  graphicsAcquireImage(context, image, meta, nullptr);
  destroyBuffer(context, stagingBuffer);

  return image;
}
