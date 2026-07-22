#pragma once

#include <cstddef>

#include <webgpu/webgpu.hpp>

#include "lewitt/render_targets.hpp"
#include "mondrian/render_contract.hpp"

namespace lewitt {
namespace render_contract {

inline bool attachment_load_store_initialized(const render_targets::attachment_state &state) {
  return state.load_op != wgpu::LoadOp::Undefined &&
         state.store_op != wgpu::StoreOp::Undefined;
}

inline bool color_attachment_initialized(const wgpu::RenderPassColorAttachment &attachment) {
  return attachment.loadOp != wgpu::LoadOp::Undefined &&
         attachment.storeOp != wgpu::StoreOp::Undefined && attachment.view != nullptr;
}

inline bool depth_attachment_initialized(const wgpu::RenderPassDepthStencilAttachment &attachment) {
  return attachment.depthLoadOp != wgpu::LoadOp::Undefined &&
         attachment.depthStoreOp != wgpu::StoreOp::Undefined && attachment.view != nullptr;
}

inline bool is_blendable_format(wgpu::TextureFormat format) {
  switch (format) {
  case wgpu::TextureFormat::RGBA8Unorm:
  case wgpu::TextureFormat::RGBA8UnormSrgb:
  case wgpu::TextureFormat::BGRA8Unorm:
  case wgpu::TextureFormat::BGRA8UnormSrgb:
  case wgpu::TextureFormat::RGBA16Float:
    return true;
  default:
    return false;
  }
}

using mondrian::render_contract::albedo_schema_uses_depth_read;
using mondrian::render_contract::gbuffer_pass_count;
using mondrian::render_contract::ssao_projected_uv_y;
using mondrian::render_contract::ssao_uniform_size_matches;

} // namespace render_contract
} // namespace lewitt
