#pragma once

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include <webgpu/webgpu.hpp>

namespace lewitt {
namespace render_targets {

using target_id = uint32_t;
constexpr target_id invalid_target_id = UINT32_MAX;

struct extent2d {
  uint32_t width = 0;
  uint32_t height = 0;
};

enum class extent_policy {
  fixed,
  swapchain,
  scaled,
};

struct attachment_state {
  wgpu::LoadOp load_op = wgpu::LoadOp::Clear;
  wgpu::StoreOp store_op = wgpu::StoreOp::Store;
  wgpu::Color clear_color{0.05f, 0.05f, 0.05f, 1.0f};
  float depth_clear = 1.0f;
  uint32_t stencil_clear = 0;
  bool depth_read_only = false;
  bool stencil_read_only = true;

  static attachment_state color_clear() {
    attachment_state state{};
    state.load_op = wgpu::LoadOp::Clear;
    state.store_op = wgpu::StoreOp::Store;
    return state;
  }

  static attachment_state depth_clear_write() {
    attachment_state state = color_clear();
    state.depth_read_only = false;
    return state;
  }

  static attachment_state depth_load_readonly() {
    attachment_state state = color_clear();
    state.load_op = wgpu::LoadOp::Load;
    state.depth_read_only = true;
    return state;
  }
};

struct render_target_desc {
  std::string label = "render_target";
  wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
  WGPUTextureUsageFlags usage = wgpu::TextureUsage::RenderAttachment;
  extent_policy size_policy = extent_policy::fixed;
  extent2d fixed_size{};
  float size_scale = 1.0f;
  uint32_t mip_level_count = 1;
  uint32_t sample_count = 1;
  wgpu::TextureAspect aspect = wgpu::TextureAspect::All;
  attachment_state attachment{};
  bool is_depth = false;
};

inline extent2d resolve_extent(const render_target_desc &desc,
                               const extent2d &swapchain_extent) {
  switch (desc.size_policy) {
  case extent_policy::fixed:
    return desc.fixed_size;
  case extent_policy::swapchain:
    return swapchain_extent;
  case extent_policy::scaled:
    return {std::max(1u, static_cast<uint32_t>(swapchain_extent.width *
                                              desc.size_scale)),
            std::max(1u, static_cast<uint32_t>(swapchain_extent.height *
                                              desc.size_scale))};
  }
  return swapchain_extent;
}

inline wgpu::TextureAspect depth_aspect(wgpu::TextureFormat format) {
  if (format == wgpu::TextureFormat::Depth24PlusStencil8 ||
      format == wgpu::TextureFormat::Depth32FloatStencil8) {
    return wgpu::TextureAspect::DepthOnly;
  }
  return wgpu::TextureAspect::DepthOnly;
}

class render_target {
public:
  render_target() = default;

  render_target(const render_target &) = delete;
  render_target &operator=(const render_target &) = delete;

  render_target(render_target &&other) noexcept
      : _desc(std::move(other._desc)), _extent(other._extent),
        _texture(other._texture), _view(other._view) {
    other._texture = nullptr;
    other._view = nullptr;
  }

  render_target &operator=(render_target &&other) noexcept {
    if (this == &other) {
      return *this;
    }
    release();
    _desc = std::move(other._desc);
    _extent = other._extent;
    _texture = other._texture;
    _view = other._view;
    other._texture = nullptr;
    other._view = nullptr;
    return *this;
  }

  bool create(wgpu::Device device, const render_target_desc &desc,
              const extent2d &extent) {
    release();
    _desc = desc;
    _extent = extent;
    if (extent.width == 0 || extent.height == 0 || desc.format == wgpu::TextureFormat::Undefined) {
      return false;
    }

    wgpu::TextureDescriptor texture_desc{};
    texture_desc.label = desc.label.c_str();
    texture_desc.dimension = wgpu::TextureDimension::_2D;
    texture_desc.format = desc.format;
    texture_desc.mipLevelCount = desc.mip_level_count;
    texture_desc.sampleCount = desc.sample_count;
    texture_desc.size = {extent.width, extent.height, 1};
    texture_desc.usage = desc.usage;
    texture_desc.viewFormatCount = 1;
    texture_desc.viewFormats = reinterpret_cast<const WGPUTextureFormat *>(&desc.format);

    _texture = device.createTexture(texture_desc);
    if (!_texture) {
      return false;
    }

    wgpu::TextureViewDescriptor view_desc{};
    if (desc.is_depth) {
      view_desc.aspect = depth_aspect(desc.format);
    } else {
      view_desc.aspect = wgpu::TextureAspect::All;
    }
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = desc.mip_level_count;
    view_desc.dimension = wgpu::TextureViewDimension::_2D;
    view_desc.format = desc.format;
    _view = _texture.createView(view_desc);
    return _view != nullptr;
  }

  bool resize(wgpu::Device device, const extent2d &extent) {
    if (_extent.width == extent.width && _extent.height == extent.height &&
        valid()) {
      return true;
    }
    return create(device, _desc, extent);
  }

  void release() {
    if (_view) {
      _view.release();
      _view = nullptr;
    }
    if (_texture) {
      _texture.destroy();
      _texture.release();
      _texture = nullptr;
    }
  }

  ~render_target() { release(); }

  bool valid() const { return _view != nullptr; }
  const render_target_desc &desc() const { return _desc; }
  const extent2d &extent() const { return _extent; }
  wgpu::TextureFormat format() const { return _desc.format; }
  wgpu::Texture texture() const { return _texture; }
  wgpu::TextureView view() const { return _view; }

  wgpu::RenderPassColorAttachment
  color_attachment(const attachment_state &state = {}) const {
    wgpu::RenderPassColorAttachment attachment{};
    attachment.setDefault();
    attachment.view = _view;
    if (attachment.view) {
      wgpuTextureViewReference(attachment.view);
    }
    attachment.resolveTarget = nullptr;
    attachment.loadOp = wgpu::LoadOp::Clear;
    attachment.storeOp = wgpu::StoreOp::Store;
    attachment.clearValue = state.clear_color;
    return attachment;
  }

  wgpu::RenderPassDepthStencilAttachment
  depth_attachment(const attachment_state &state = {}) const {
    wgpu::RenderPassDepthStencilAttachment attachment{};
    attachment.setDefault();
    attachment.view = _view;
    if (attachment.view) {
      wgpuTextureViewReference(attachment.view);
    }
    attachment.depthClearValue = state.depth_clear;
    attachment.depthLoadOp =
        state.depth_read_only ? wgpu::LoadOp::Load : wgpu::LoadOp::Clear;
    attachment.depthStoreOp = wgpu::StoreOp::Store;
    attachment.depthReadOnly = state.depth_read_only;
    attachment.stencilClearValue = state.stencil_clear;
    attachment.stencilLoadOp = wgpu::LoadOp::Clear;
    attachment.stencilStoreOp = wgpu::StoreOp::Store;
    attachment.stencilReadOnly = state.stencil_read_only;
    return attachment;
  }

private:
  render_target_desc _desc{};
  extent2d _extent{};
  wgpu::Texture _texture = nullptr;
  wgpu::TextureView _view = nullptr;
};

class target_pool {
public:
  target_id emplace(wgpu::Device device, const render_target_desc &desc,
                    const extent2d &extent) {
    if (_targets.capacity() == _targets.size()) {
      _targets.reserve(std::max<std::size_t>(_targets.size() + 1, 4));
    }
    const target_id id = static_cast<target_id>(_targets.size());
    _targets.emplace_back();
    if (!_targets.back().create(device, desc, extent)) {
      _targets.pop_back();
      return invalid_target_id;
    }
    return id;
  }

  bool resize(target_id id, wgpu::Device device, const extent2d &extent) {
    if (id >= _targets.size()) {
      return false;
    }
    return _targets[id].resize(device, extent);
  }

  void resize_all(wgpu::Device device, const extent2d &swapchain_extent) {
    for (auto &target : _targets) {
      const extent2d extent =
          resolve_extent(target.desc(), swapchain_extent);
      target.resize(device, extent);
    }
  }

  void clear() {
    for (auto &target : _targets) {
      target.release();
    }
    _targets.clear();
  }

  render_target &get(target_id id) { return _targets.at(id); }
  const render_target &get(target_id id) const { return _targets.at(id); }

  bool valid(target_id id) const {
    return id < _targets.size() && _targets[id].valid();
  }

  std::size_t size() const { return _targets.size(); }

private:
  std::vector<render_target> _targets;
};

struct external_color_attachment {
  wgpu::TextureView view = nullptr;
  attachment_state state{};
  bool release_view_after_pass = true;
};

struct attachment_view {
  wgpu::TextureView view = nullptr;
  attachment_state state{};
  bool borrowed = false;
};

struct sampled_texture_view {
  wgpu::TextureView view = nullptr;
  wgpu::TextureSampleType sample_type = wgpu::TextureSampleType::Float;
  wgpu::TextureViewDimension dimension = wgpu::TextureViewDimension::_2D;
};

inline attachment_view as_attachment_view(const external_color_attachment &color) {
  attachment_view view{};
  view.view = color.view;
  view.state = color.state;
  view.borrowed = true;
  return view;
}

inline attachment_view as_attachment_view(const render_target &target) {
  attachment_view view{};
  view.view = target.view();
  view.state = target.desc().attachment;
  view.borrowed = false;
  return view;
}

inline sampled_texture_view as_sampled_view(const render_target &target) {
  sampled_texture_view view{};
  view.view = target.view();
  view.sample_type = wgpu::TextureSampleType::Float;
  view.dimension = wgpu::TextureViewDimension::_2D;
  return view;
}

struct frame_attachments {
  external_color_attachment color{};
  target_id color_target = invalid_target_id;
  std::vector<target_id> color_targets{};
  std::vector<wgpu::RenderPassColorAttachment> pooled_colors{};
  target_id depth_target = invalid_target_id;
  wgpu::TextureView depth_view = nullptr;
  attachment_state depth_state{};
};

inline frame_attachments resolve_attachments(target_pool *target_pool,
                                             frame_attachments attachments) {
  if (!target_pool) {
    return attachments;
  }

  if (attachments.color_target != invalid_target_id &&
      target_pool->valid(attachments.color_target)) {
    const auto &color = target_pool->get(attachments.color_target);
    attachments.color.view = color.view();
    attachments.color.release_view_after_pass = false;
    if (attachments.color.state.load_op == wgpu::LoadOp::Undefined) {
      attachments.color.state = color.desc().attachment;
    }
  }

  if (!attachments.color_targets.empty()) {
    attachments.pooled_colors.clear();
    attachments.pooled_colors.reserve(attachments.color_targets.size());
    for (const auto target_id : attachments.color_targets) {
      if (!target_pool->valid(target_id)) {
        continue;
      }
      const auto &color = target_pool->get(target_id);
      attachments.pooled_colors.push_back(
          color.color_attachment(color.desc().attachment));
    }
  }

  if (attachments.depth_target != invalid_target_id &&
      target_pool->valid(attachments.depth_target)) {
    const auto &depth = target_pool->get(attachments.depth_target);
    attachments.depth_view = depth.view();
    if (attachments.depth_view) {
      wgpuTextureViewReference(attachments.depth_view);
    }
    if (attachments.depth_state.load_op == wgpu::LoadOp::Undefined) {
      attachments.depth_state = depth.desc().attachment;
    }
  }

  return attachments;
}

inline render_target_desc depth_target_desc(wgpu::TextureFormat format =
                                                wgpu::TextureFormat::Depth24Plus) {
  render_target_desc desc;
  desc.label = "depth";
  desc.format = format;
  desc.usage = wgpu::TextureUsage::RenderAttachment;
  desc.size_policy = extent_policy::swapchain;
  desc.is_depth = true;
  desc.attachment = attachment_state::depth_clear_write();
  desc.attachment.depth_clear = 1.0f;
  return desc;
}

} // namespace render_targets
} // namespace lewitt
