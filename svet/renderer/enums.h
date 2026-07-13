#pragma once
#include <cstdint>

namespace svet::renderer {

enum class MemoryProperties : uint8_t {
  DEVICE_LOCAL = 1 << 0,
  HOST_VISIBLE = 1 << 1,
  HOST_COHERENT = 1 << 2,
  HOST_CACHED = 1 << 3,
};
inline constexpr MemoryProperties operator|(MemoryProperties lhs,
                                            MemoryProperties rhs) {
  return static_cast<MemoryProperties>(static_cast<uint8_t>(lhs) |
                                       static_cast<uint8_t>(rhs));
}
inline constexpr MemoryProperties operator&(MemoryProperties lhs,
                                            MemoryProperties rhs) {
  return static_cast<MemoryProperties>(static_cast<uint8_t>(lhs) &
                                       static_cast<uint8_t>(rhs));
}
inline constexpr MemoryProperties operator^(MemoryProperties lhs,
                                            MemoryProperties rhs) {
  return static_cast<MemoryProperties>(static_cast<uint8_t>(lhs) ^
                                       static_cast<uint8_t>(rhs));
}
inline constexpr MemoryProperties operator~(MemoryProperties flags) {
  return static_cast<MemoryProperties>(~static_cast<uint8_t>(flags));
}

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

enum class VertexFormat {
  FLOAT = 100,
  FLOAT2 = 103,
  FLOAT3 = 106,
  FLOAT4 = 109,
  INT = 99,
  INT2 = 102,
  INT3 = 105,
  INT4 = 108,
  UINT = 98,
  UINT2 = 101,
  UINT3 = 104,
  UINT4 = 107
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
  UINT8_RGBA_SRGB = 43,
  UINT8_BGRA_SRGB = 50,
  UINT8_RGBA = 41,
  UINT8_BGRA = 48,
  UINT8_RGB_SRGB = 29,
  UINT8_BGR_SRGB = 36,
  UINT8_RGB = 27,
  UINT8_BGR = 34,
  UNORM8_RGBA = 37,
  UNORM8_BGRA = 44,
  UNORM8_RGB = 23,
  UNORM8_BGR = 30,
  UNORM8_R = 9,
  UINT8_R = 13,
  UINT8_R_SRGB = 15,
  UINT16_RGB = 88,
  UINT16_RGBA = 95,
  UNORM16_R = 70,
  UNORM16_RGB = 84,
  UNORM16_RGBA = 91,
  UINT16_R = 74,
  UINT32_R = 98,
  UINT32_RGB = 104,
  UINT32_RGBA = 107,
  FLOAT16_R = 76,
  FLOAT16_RGB = 90,
  FLOAT16_RGBA = 97,
  FLOAT32_R = 100,
  FLOAT32_RGB = 106,
  FLOAT32_RGBA = 109,
  UNORM16_D = 124,
  FLOAT32_D = 126,
  BC1_RGBA = 131,
  BC1_RGB = 132,
  BC1_RGBA_SRGB = 133,
  BC1_RGB_SRGB = 134,
  BC2 = 135,
  BC2_SRGB = 136,
  BC3 = 137,
  BC3_SRGB = 138,
  BC4 = 139,
  BC4_S = 140,
  BC5 = 141,
  BC5_S = 142,
  BC6H = 143,
  BC6H_S = 144,
  BC7 = 145,
  BC7_SRGB = 146,
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

enum class QueueOwnership : uint8_t {
  GRAPHICS = 0,
  TRANSFER = 1,
};

enum class QueueOwnershipState : uint8_t {
  GRAPHICS = 0,
  TRANSFER = 1,
  GRAPHICS_RELEASED_TO_TRANSFER = 10,
  TRANSFER_RELEASED_TO_GRAPHICS = 11,
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

} // namespace svet::renderer
