#include "image-gen.h"
#include "svet/renderer/staging.h"

svet::renderer::Image
genCheckerPattern(svet::renderer::LContext context, uint32_t width,
                  uint32_t height, svet::renderer::StagingBuffer stagingBuffer,
                  svet::renderer::MemoryPool textureImagePool) {
  using namespace svet::renderer;

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

  ImageMetadata meta;
  ImageSpecification imageSpec{};
  imageSpec.outMeta = &meta;
  imageSpec.width = imageWidth;
  imageSpec.height = imageHeight;
  imageSpec.memoryPool = textureImagePool;
  imageSpec.pixelFormat = PixelFormat::UNORM8_RGBA;
  imageSpec.usage = ImageUsage::TRANSFER_DST | ImageUsage::SAMPLED;
  imageSpec.tiling = ImageTiling::OPTIMAL;
  imageSpec.initialOwnership = QueueOwnership::TRANSFER;
  Image image = createImage(context, imageSpec);
  StagedUploadImageSpecification stagedUploadSpec{};
  stagedUploadSpec.width = imageWidth;
  stagedUploadSpec.height = imageHeight;
  stagedUploadSpec.data = imageData;
  stagedUploadSpec.pixelSize = pixelSize;
  stagedUploadSpec.stagingBuffer = stagingBuffer;
  stagedUploadSpec.destination = image;
  stagedUploadSpec.destinationMeta = meta;
  stagedUploadSpec.desiredLayout = ImageLayout::SHADER_READ_ONLY;
  stagedUploadSpec.desiredOwnership = QueueOwnership::GRAPHICS;
  stagedUploadImageData(context, stagedUploadSpec);

  delete[] imageData;

  return image;
}

svet::renderer::Image genGradient(svet::renderer::LContext context,
                                  uint32_t width, uint32_t height,
                                  svet::renderer::StagingBuffer stagingBuffer,
                                  svet::renderer::MemoryPool textureImagePool) {
  using namespace svet::renderer;

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
  ImageMetadata intermediateMeta;
  ImageSpecification imageSpec{};
  imageSpec.outMeta = &intermediateMeta;
  imageSpec.width = imageWidth;
  imageSpec.height = imageHeight;
  // TODO: This means the intermediate texture memory will not be returned to
  // the pool (pools cannot free right now)
  imageSpec.memoryPool = textureImagePool;
  imageSpec.pixelFormat = PixelFormat::UNORM8_RGBA;
  imageSpec.usage =
      ImageUsage::TRANSFER_SRC | ImageUsage::TRANSFER_DST | ImageUsage::SAMPLED;
  imageSpec.tiling = ImageTiling::OPTIMAL;
  imageSpec.initialOwnership = QueueOwnership::TRANSFER;
  Image intermediateImage = createImage(context, imageSpec);
  StagedUploadImageSpecification stagedUploadSpec{};
  stagedUploadSpec.width = imageWidth;
  stagedUploadSpec.height = imageHeight;
  stagedUploadSpec.data = fourPixels;
  stagedUploadSpec.pixelSize = 4;
  stagedUploadSpec.stagingBuffer = stagingBuffer;
  stagedUploadSpec.destination = intermediateImage;
  stagedUploadSpec.destinationMeta = intermediateMeta;
  stagedUploadSpec.outDestinationMeta = &intermediateMeta;
  stagedUploadSpec.desiredLayout = ImageLayout::TRANSFER_SRC;
  stagedUploadSpec.desiredOwnership = QueueOwnership::TRANSFER;
  stagedUploadImageData(context, stagedUploadSpec);

  ImageMetadata meta;
  imageSpec.outMeta = &meta;
  imageSpec.width = finalImageWidth;
  imageSpec.height = finalImageHeight;
  imageSpec.memoryPool = textureImagePool;
  imageSpec.usage = ImageUsage::TRANSFER_DST | ImageUsage::SAMPLED;
  Image image = createImage(context, imageSpec);

  ImageBlitSpecification blitSpec{};
  blitSpec.source = intermediateImage;
  blitSpec.sourceMeta = intermediateMeta;
  blitSpec.destination = image;
  blitSpec.destinationMeta = meta;
  blitSpec.outDestinationMeta = &meta;
  blitSpec.filter = SampleFilter::LINEAR;
  blitSpec.desiredSrcLayout = ImageLayout::UNDEFINED;
  blitSpec.desiredSrcOwnership = QueueOwnership::TRANSFER;
  blitSpec.desiredDstLayout = ImageLayout::SHADER_READ_ONLY;
  blitSpec.desiredDstOwnership = QueueOwnership::GRAPHICS;
  blitImage(context, blitSpec);

  destroyImage(context, intermediateImage);

  return image;
}

svet::renderer::Image
genDefaultNormal(svet::renderer::LContext context,
                 svet::renderer::StagingBuffer stagingBuffer,
                 svet::renderer::MemoryPool textureImagePool) {
  return genSinglePixel(context, 127, 127, 255, stagingBuffer,
                        textureImagePool);
}

svet::renderer::Image
genSinglePixel(svet::renderer::LContext context, uint8_t r, uint8_t g,
               uint8_t b, svet::renderer::StagingBuffer stagingBuffer,
               svet::renderer::MemoryPool textureImagePool) {
  using namespace svet::renderer;

  uint8_t pixel[] = {r, g, b, 255};

  ImageMetadata meta;
  ImageSpecification imageSpec{};
  imageSpec.outMeta = &meta;
  imageSpec.width = 1;
  imageSpec.height = 1;
  imageSpec.memoryPool = textureImagePool;
  imageSpec.pixelFormat = PixelFormat::UNORM8_RGBA;
  imageSpec.usage = ImageUsage::TRANSFER_DST | ImageUsage::SAMPLED;
  imageSpec.tiling = ImageTiling::OPTIMAL;
  imageSpec.initialOwnership = QueueOwnership::TRANSFER;
  Image image = createImage(context, imageSpec);
  StagedUploadImageSpecification stagedUploadSpec{};
  stagedUploadSpec.width = 1;
  stagedUploadSpec.height = 1;
  stagedUploadSpec.data = pixel;
  stagedUploadSpec.pixelSize = 4;
  stagedUploadSpec.stagingBuffer = stagingBuffer;
  stagedUploadSpec.destination = image;
  stagedUploadSpec.destinationMeta = meta;
  stagedUploadSpec.desiredLayout = ImageLayout::SHADER_READ_ONLY;
  stagedUploadSpec.desiredOwnership = QueueOwnership::GRAPHICS;
  stagedUploadImageData(context, stagedUploadSpec);

  return image;
}
