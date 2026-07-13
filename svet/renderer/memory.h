#pragma once
#include "context.h"
#include "enums.h"

#include <cstddef>

namespace svet::renderer {

using MemoryPool = struct MemoryPoolT *;

struct MemoryPoolSpecification {
  size_t size;
  MemoryProperties properties;
  BufferUsage bufferUsage;
  ImageUsage imageUsage;
  ImageTiling imageTiling;
};
MemoryPool createMemoryPool(LContext context,
                            const MemoryPoolSpecification &spec);
void destroyMemoryPool(LContext context, MemoryPool memoryPool);

} // namespace svet::renderer
