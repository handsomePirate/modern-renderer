#pragma once
#include "renderer.h"

Image genCheckerPattern(LContext context, uint32_t width, uint32_t height);
Image genGradient(LContext context, uint32_t width, uint32_t height);

Image genDefaultNormal(LContext context);
Image genSinglePixel(LContext context, uint8_t r, uint8_t g, uint8_t b);
