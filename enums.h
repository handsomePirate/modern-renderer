#pragma once
#include <cstdint>

// Determines where the buffer data originates
enum class BufferSource { CPU, GPU };

// Determines who reads the buffer
enum class BufferConsumer { GPU, CPU };

// What GPU operations use this buffer (can be multiple)
enum class BufferUsage : uint16_t {
  NONE = 0,
  TRANSFER_SRC = 1 << 0,
  TRANSFER_DST = 1 << 1,
  UNIFORM_TEXEL = 1 << 2,
  STORAGE_TEXEL = 1 << 3,
  UNIFORM = 1 << 4,
  STORAGE = 1 << 5,
  INDEX = 1 << 6,
  VERTEX = 1 << 7,
  INDIRECT = 1 << 8,
};
inline constexpr BufferUsage operator|(BufferUsage lhs, BufferUsage rhs) {
  return static_cast<BufferUsage>(static_cast<uint16_t>(lhs) |
                                  static_cast<uint16_t>(rhs));
}
inline constexpr BufferUsage operator&(BufferUsage lhs, BufferUsage rhs) {
  return static_cast<BufferUsage>(static_cast<uint16_t>(lhs) &
                                  static_cast<uint16_t>(rhs));
}
inline constexpr BufferUsage operator^(BufferUsage lhs, BufferUsage rhs) {
  return static_cast<BufferUsage>(static_cast<uint16_t>(lhs) ^
                                  static_cast<uint16_t>(rhs));
}
inline constexpr BufferUsage operator~(BufferUsage flags) {
  return static_cast<BufferUsage>(~static_cast<uint16_t>(flags));
}

// How often the data changes (tuning hint)
enum class BufferFrequency {
  STATIC,  // Written once or very infrequently
  DYNAMIC, // Modified occasionally
  STREAM   // Modified frequently in tight loops
};

enum class VertexFormat {
  FLOAT,
  FLOAT2,
  FLOAT3,
  FLOAT4,
  INT,
  INT2,
  INT3,
  INT4,
  UINT,
  UINT2,
  UINT3,
  UINT4
};

enum class ResourceType {
  UNIFORM_BUFFER,
  SAMPLER,
  SAMPLED_IMAGE,
  STORAGE_IMAGE,
  COMBINED_SAMPLER,
  STORAGE_BUFFER,
  STORAGE_TEXEL_BUFFER,
};

enum class ShaderVisibility : uint32_t {
  NONE = 0,
  VERTEX = 1 << 0,
  TESSELLATION_CONTROL = 1 << 1,
  TESSELLATION_EVALUATION = 1 << 2,
  GEOMETRY = 1 << 3,
  FRAGMENT = 1 << 4,
  COMPUTE = 1 << 5,
  /*
 RAY_GENERATION,
 RAY_ANY_HIT,
 RAY_CLOSEST_HIT,
 RAY_MISS,
 INTERSECTION,
 CALLABLE,
 TASK,
 MESH,
 */
};
inline constexpr ShaderVisibility operator|(ShaderVisibility lhs,
                                            ShaderVisibility rhs) {
  return static_cast<ShaderVisibility>(static_cast<uint32_t>(lhs) |
                                       static_cast<uint32_t>(rhs));
}
inline constexpr ShaderVisibility operator&(ShaderVisibility lhs,
                                            ShaderVisibility rhs) {
  return static_cast<ShaderVisibility>(static_cast<uint32_t>(lhs) &
                                       static_cast<uint32_t>(rhs));
}
inline constexpr ShaderVisibility operator^(ShaderVisibility lhs,
                                            ShaderVisibility rhs) {
  return static_cast<ShaderVisibility>(static_cast<uint32_t>(lhs) ^
                                       static_cast<uint32_t>(rhs));
}
inline constexpr ShaderVisibility operator~(ShaderVisibility flags) {
  return static_cast<ShaderVisibility>(~static_cast<uint32_t>(flags));
}

enum class VertexInputRate { VERTEX, INSTANCE };

enum class DrawMode { TRIANGLES, TRIANGLE_STRIP };

enum class LoadOp { NONE, DONT_CARE, CLEAR, LOAD };

enum class StoreOp { NONE, DONT_CARE, STORE };

enum class PixelFormat {
  UINT8_RGBA_SRGB,
  UINT8_BGRA_SRGB,
  UINT8_RGBA,
  UINT8_BGRA,
  UINT8_RGB_SRGB,
  UINT8_BGR_SRGB,
  UINT8_RGB,
  UINT8_BGR,
  UNORM8_RGBA,
  UNORM8_BGRA,
  UNORM8_RGB,
  UNORM8_BGR,
  UINT8_R_SRGB,
  UINT8_R,
  UNORM8_R,
  UINT16_RGBA,
  UINT16_RGB,
  UNORM16_RGBA,
  UNORM16_RGB,
  UINT16_R,
  UNORM16_R,
  UINT32_RGBA,
  UINT32_RGB,
  UINT32_R,
  FLOAT32_RGBA,
  FLOAT32_RGB,
  FLOAT32_R,
  FLOAT16_RGBA,
  FLOAT16_RGB,
  FLOAT16_R,
  UNORM16_D,
  FLOAT32_D,
  // TODO: Block compressions
};

enum class ImageUsage : uint32_t {
  NONE = 0,
  TRANSFER_SRC = 1 << 0,
  TRANSFER_DST = 1 << 1,
  SAMPLED = 1 << 2,
  STORAGE = 1 << 3,
  RENDER_TARGET_COLOR = 1 << 4,
  RENDER_TARGET_DEPTH_STENCIL = 1 << 5,
};
inline constexpr ImageUsage operator|(ImageUsage lhs, ImageUsage rhs) {
  return static_cast<ImageUsage>(static_cast<uint32_t>(lhs) |
                                 static_cast<uint32_t>(rhs));
}
inline constexpr ImageUsage operator&(ImageUsage lhs, ImageUsage rhs) {
  return static_cast<ImageUsage>(static_cast<uint32_t>(lhs) &
                                 static_cast<uint32_t>(rhs));
}
inline constexpr ImageUsage operator^(ImageUsage lhs, ImageUsage rhs) {
  return static_cast<ImageUsage>(static_cast<uint32_t>(lhs) ^
                                 static_cast<uint32_t>(rhs));
}
inline constexpr ImageUsage operator~(ImageUsage flags) {
  return static_cast<ImageUsage>(~static_cast<uint32_t>(flags));
}

enum class PipelineStage : uint64_t {
  NONE = 0,
  TOP_OF_PIPE = 1 << 0,
  VERTEX_SHADER = 1 << 3,
  FRAGMENT_SHADER = 1 << 7,
  EARLY_FRAGMENT_TESTS = 1 << 8,
  LATE_FRAGMENT_TESTS = 1 << 9,
  RENDER_TARGET_OUTPUT = 1 << 10,
  COMPUTE_SHADER = 1 << 11,
  TRANSFER = 1 << 12,
  BOTTOM_OF_PIPE = 1 << 13,
  VERTEX_ATTRIBUTE_INPUT = 1ull << 37,
};
inline constexpr PipelineStage operator|(PipelineStage lhs, PipelineStage rhs) {
  return static_cast<PipelineStage>(static_cast<uint64_t>(lhs) |
                                    static_cast<uint64_t>(rhs));
}
inline constexpr PipelineStage operator&(PipelineStage lhs, PipelineStage rhs) {
  return static_cast<PipelineStage>(static_cast<uint64_t>(lhs) &
                                    static_cast<uint64_t>(rhs));
}
inline constexpr PipelineStage operator^(PipelineStage lhs, PipelineStage rhs) {
  return static_cast<PipelineStage>(static_cast<uint64_t>(lhs) ^
                                    static_cast<uint64_t>(rhs));
}
inline constexpr PipelineStage operator~(PipelineStage flags) {
  return static_cast<PipelineStage>(~static_cast<uint64_t>(flags));
}

enum class ResourceAccess : uint64_t {
  NONE = 0,
  INDEX_READ = 1 << 1,
  VERTEX_ATTRIBUTE_READ = 1 << 2,
  UNIFORM_READ = 1 << 3,
  SHADER_READ = 1 << 5,
  SHADER_WRITE = 1 << 6,
  RENDER_TARGET_READ = 1 << 7,
  RENDER_TARGET_WRITE = 1 << 8,
  DEPTH_STENCIL_READ = 1 << 9,
  DEPTH_STENCIL_WRITE = 1 << 10,
  TRANSFER_READ = 1 << 11,
  TRANSFER_WRITE = 1 << 12,
};
inline constexpr ResourceAccess operator|(ResourceAccess lhs,
                                          ResourceAccess rhs) {
  return static_cast<ResourceAccess>(static_cast<uint64_t>(lhs) |
                                     static_cast<uint64_t>(rhs));
}
inline constexpr ResourceAccess operator&(ResourceAccess lhs,
                                          ResourceAccess rhs) {
  return static_cast<ResourceAccess>(static_cast<uint64_t>(lhs) &
                                     static_cast<uint64_t>(rhs));
}
inline constexpr ResourceAccess operator^(ResourceAccess lhs,
                                          ResourceAccess rhs) {
  return static_cast<ResourceAccess>(static_cast<uint64_t>(lhs) ^
                                     static_cast<uint64_t>(rhs));
}
inline constexpr ResourceAccess operator~(ResourceAccess flags) {
  return static_cast<ResourceAccess>(~static_cast<uint64_t>(flags));
}

enum class SampleFilter {
  NEAREST,
  LINEAR,
};

enum class FaceCulling { NONE, FRONT, BACK };

enum class SampleAddressing {
  REPEAT,
  MIRRORED_REPEAT,
  CLAMP_TO_EDGE,
  CLAMP_TO_BORDER,
  MIRROR_CLAMP_TO_EDGE,
};

enum class BlendOp { ADD, SUBTRACT, MIN, MAX };

enum class BlendFactor {
  ZERO,
  ONE,
  SRC_COLOR,
  ONE_MINUS_SRC_COLOR,
  DST_COLOR,
  ONE_MINUS_DST_COLOR,
  SRC_ALPHA,
  ONE_MINUS_SRC_ALPHA,
  DST_ALPHA,
  ONE_MINUS_DST_ALPHA,
  CONSTANT_COLOR,
  ONE_MINUS_CONSTANT_COLOR,
  CONSTANT_ALPHA,
  ONE_MINUS_CONSTANT_ALPHA,
  SRC_ALPHA_SATURATE,
  SRC1_COLOR,
  ONE_MINUS_SRC1_COLOR,
  SRC1_ALPHA,
  ONE_MINUS_SRC1_ALPHA,
};

enum class ColorComponent : uint8_t { R = 1, G = 2, B = 4, A = 8 };

inline constexpr ColorComponent operator|(ColorComponent lhs,
                                          ColorComponent rhs) {
  return static_cast<ColorComponent>(static_cast<uint8_t>(lhs) |
                                     static_cast<uint8_t>(rhs));
}
inline constexpr ColorComponent operator&(ColorComponent lhs,
                                          ColorComponent rhs) {
  return static_cast<ColorComponent>(static_cast<uint8_t>(lhs) &
                                     static_cast<uint8_t>(rhs));
}
inline constexpr ColorComponent operator^(ColorComponent lhs,
                                          ColorComponent rhs) {
  return static_cast<ColorComponent>(static_cast<uint8_t>(lhs) ^
                                     static_cast<uint8_t>(rhs));
}
inline constexpr ColorComponent operator~(ColorComponent flags) {
  return static_cast<ColorComponent>(~static_cast<uint8_t>(flags));
}

// IMPORTANT: Modification should prompt the following changes:
// - renderer.h - DrawCommand
// - renderer.cpp - dispatchProcessing
// - types.h - DrawCommandIndexes
// - main.cpp - preallocated memory
enum class DrawOperationType : uint32_t {
  BEGIN_RENDER_PASS = 0,
  END_RENDER_PASS = 1,
  IMAGE_BARRIER = 2,
  BUFFER_BARRIER = 3,
  BIND_PIPELINE = 4,
  BIND_DESCRIPTOR_SETS = 5,
  PUSH_CONSTANT = 6,
  BIND_VERTEX_BUFFER = 7,
  BIND_INDEX_BUFFER = 8,
  DRAW = 9,
  DISPATCH = 10,
  BLIT = 11,
};

enum class ImageLayout {
  UNDEFINED,
  GENERAL,
  COLOR_RENDER_TARGET,
  DEPTH_RENDER_TARGET,
  SHADER_READ_ONLY,
  TRANSFER_SRC,
  TRANSFER_DST,
  PRESENT,
};

enum class QueueOwnership {
  NONE,
  GRAPHICS,
  TRANSFER,
  GRAPHICS_RELEASED_TO_TRANSFER,
  TRANSFER_RELEASED_TO_GRAPHICS,
};

enum class ImageTiling { OPTIMAL, LINEAR };

enum class RenderTargetType {
  COLOR,
  DEPTH,
};

enum class FrontFaceWind {
  CLOCKWISE,
  COUNTER_CLOCKWISE,
};
