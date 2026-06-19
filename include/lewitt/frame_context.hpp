#pragma once

#include <memory>

#include <webgpu/webgpu.hpp>

#include "lewitt/render_targets.hpp"

namespace lewitt {

struct swapchain_target {
  render_targets::external_color_attachment color{};
  render_targets::target_id depth = render_targets::invalid_target_id;
};

struct frame_context {
  wgpu::Device device = nullptr;
  wgpu::Queue queue = nullptr;
  wgpu::SwapChain swapchain = nullptr;
  render_targets::target_pool &texture_targets;
  std::shared_ptr<void> buffer_targets;
  swapchain_target external_swapchain{};

  swapchain_target &swapchain_attachment() { return external_swapchain; }
  const swapchain_target &swapchain_attachment() const { return external_swapchain; }
};

} // namespace lewitt
