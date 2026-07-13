#pragma once
#include "context.h"

#include <cstddef>
#include <cstdint>

namespace svet::renderer {

using Shader = struct ShaderT *;

Shader loadSPIRVShader(LContext context, uint8_t bytes[], size_t size);
void destroyShader(LContext context, Shader shader);

} // namespace svet::renderer
