#pragma once

#include <webgpu/webgpu.hpp>

#include "lewitt/render_targets.hpp"

namespace lewitt {

struct present_target {
  wgpu::SwapChain swapchain = nullptr;
  wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
  render_targets::extent2d extent{};

  static present_target from_context(wgpu::SwapChain swapchain,
                                     wgpu::TextureFormat format,
                                     const render_targets::extent2d &extent) {
    present_target target{};
    target.swapchain = swapchain;
    target.format = format;
    target.extent = extent;
    return target;
  }
};

} // namespace lewitt
