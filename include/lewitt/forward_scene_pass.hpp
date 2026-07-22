#pragma once

#include <functional>
#include <iostream>

#include <webgpu/webgpu.hpp>

#include "lewitt/passes.hpp"
#include "lewitt/performance.hpp"
#include "lewitt/render_targets.hpp"
#include "lewitt/scene.hpp"

namespace lewitt {
namespace pipeline {

// Swapchain forward pass: color + depth targets, optional background draw, then scene renderables.
class forward_scene_pass {
public:
  bool init(wgpu::Device device, const render_targets::extent2d &extent,
            wgpu::TextureFormat color_format, wgpu::TextureFormat depth_format) {
    _device = device;
    _extent = extent;
    _color_format = color_format;
    _depth_format = depth_format;
    return init_depth_texture();
  }

  void resize(wgpu::Device device, const render_targets::extent2d &extent) {
    _device = device;
    _extent = extent;
    terminate_depth_texture();
    init_depth_texture();
  }

  void terminate() { terminate_depth_texture(); }

  void execute(
      wgpu::SwapChain swapchain, wgpu::Device &device, const render_scene::ptr &scene,
      const std::function<void(wgpu::RenderPassEncoder &, wgpu::Device &)> &background = nullptr) {
    LEWITT_PERF_SCOPE_PATH("lewitt::pipeline::forward_scene_pass::execute");
    if (!scene || !_depth_view) {
      return;
    }

    render_targets::frame_attachments attachments{};
    attachments.depth_view = _depth_view;
    attachments.depth_state.depth_clear = 1.0f;
    attachments.depth_state.load_op = wgpu::LoadOp::Clear;
    attachments.depth_state.store_op = wgpu::StoreOp::Store;

    passes::render(
        swapchain, device, attachments,
        [&](wgpu::RenderPassEncoder &enc, wgpu::Device &dev) {
          if (background) {
            background(enc, dev);
          }
          scene->render(enc, dev);
        },
        true);
  }

private:
  bool init_depth_texture() {
    if (!_device || !_extent.width || !_extent.height ||
        _depth_format == wgpu::TextureFormat::Undefined) {
      return false;
    }

    wgpu::TextureDescriptor depth_desc{};
    depth_desc.dimension = wgpu::TextureDimension::_2D;
    depth_desc.format = _depth_format;
    depth_desc.mipLevelCount = 1;
    depth_desc.sampleCount = 1;
    depth_desc.size = {_extent.width, _extent.height, 1};
    depth_desc.usage = wgpu::TextureUsage::RenderAttachment;
    depth_desc.viewFormatCount = 1;
    depth_desc.viewFormats = reinterpret_cast<const WGPUTextureFormat *>(&_depth_format);

    _depth_texture = _device.createTexture(depth_desc);
    if (!_depth_texture) {
      std::cerr << "forward_scene_pass: failed to create depth texture" << std::endl;
      return false;
    }

    wgpu::TextureViewDescriptor view_desc{};
    view_desc.aspect = wgpu::TextureAspect::DepthOnly;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.dimension = wgpu::TextureViewDimension::_2D;
    view_desc.format = _depth_format;
    _depth_view = _depth_texture.createView(view_desc);
    return _depth_view != nullptr;
  }

  void terminate_depth_texture() {
    if (_depth_view) {
      _depth_view.release();
      _depth_view = nullptr;
    }
    if (_depth_texture) {
      _depth_texture.destroy();
      _depth_texture.release();
      _depth_texture = nullptr;
    }
  }

  wgpu::Device _device = nullptr;
  render_targets::extent2d _extent{};
  wgpu::TextureFormat _color_format = wgpu::TextureFormat::Undefined;
  wgpu::TextureFormat _depth_format = wgpu::TextureFormat::Undefined;
  wgpu::Texture _depth_texture = nullptr;
  wgpu::TextureView _depth_view = nullptr;
};

} // namespace pipeline
} // namespace lewitt
