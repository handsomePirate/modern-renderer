#pragma once
#include "svet/renderer/context.h"
#include "svet/renderer/image.h"
#include "svet/renderer/staging.h"

svet::renderer::Image
genCheckerPattern(svet::renderer::LContext context, uint32_t width,
                  uint32_t height, svet::renderer::StagingBuffer stagingBuffer,
                  svet::renderer::MemoryPool textureImagePool);
svet::renderer::Image genGradient(svet::renderer::LContext context,
                                  uint32_t width, uint32_t height,
                                  svet::renderer::StagingBuffer stagingBuffer,
                                  svet::renderer::MemoryPool textureImagePool);

svet::renderer::Image
genDefaultNormal(svet::renderer::LContext context,
                 svet::renderer::StagingBuffer stagingBuffer,
                 svet::renderer::MemoryPool textureImagePool);
svet::renderer::Image
genSinglePixel(svet::renderer::LContext context, uint8_t r, uint8_t g,
               uint8_t b, svet::renderer::StagingBuffer stagingBuffer,
               svet::renderer::MemoryPool textureImagePool);
