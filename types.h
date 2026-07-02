#pragma once
#include <cstdint>

struct DrawCommandIndexes {
  uint32_t operationIndex;
  uint32_t renderPassIndex;
  uint32_t pipelineIndex;
  uint32_t descriptorSetBindingIndex;
  uint32_t pushConstantIndex;
  uint32_t imageBarrierIndex;
  uint32_t bufferBarrierIndex;
  uint32_t vertexBindingIndex;
  uint32_t indexBindingIndex;
  uint32_t drawCallIndex;
  uint32_t boundDrawCallIndex;
  uint32_t dispatchCallIndex;
  uint32_t blitCallIndex;
};
