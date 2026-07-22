#pragma once

#include <cstddef>
#include <cstdint>

#include <webgpu/webgpu.hpp>

namespace mondrian {
namespace render_contract {

inline bool ssao_uniform_size_matches(std::size_t size) {
  // WGSL SsaoParams: mat4 projectionMatrix + vec4 params
  return size == 80;
}

inline int gbuffer_pass_count(bool has_meshes, bool has_debug_lines) {
  int passes = 0;
  if (has_meshes || has_debug_lines) {
    passes += 1; // MRT position/normal + depth
    passes += 1; // albedo with depth read
  }
  return passes;
}

inline wgpu::CompareFunction albedo_depth_compare() {
  return wgpu::CompareFunction::LessEqual;
}

inline bool albedo_schema_uses_depth_read(const wgpu::CompareFunction depth_compare,
                                          bool depth_write_enabled) {
  return !depth_write_enabled && depth_compare == wgpu::CompareFunction::LessEqual;
}

// SSAO projected sample UV flips Y when converting from NDC to texture space.
inline float ssao_projected_uv_y(float ndc_y) { return 1.0f - (ndc_y * 0.5f + 0.5f); }

} // namespace render_contract
} // namespace mondrian
