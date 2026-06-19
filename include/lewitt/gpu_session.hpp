#pragma once

#include <cstdint>
#include <functional>
#include <functional>
#include <memory>
#include <string>
#include <cstdint>

#include <webgpu/webgpu.hpp>

#include "lewitt/render_targets.hpp"
#include "lewitt/scene.hpp"

struct GLFWwindow;

namespace lewitt {

struct base_config {
  uint32_t width = 1280;
  uint32_t height = 720;
  std::string title = "Vermeer";
};

struct gpu_context {
  GLFWwindow *window = nullptr;
  wgpu::Device device = nullptr;
  wgpu::Queue queue = nullptr;
  wgpu::SwapChain swapchain = nullptr;
  wgpu::TextureFormat swapchain_format = wgpu::TextureFormat::Undefined;
  wgpu::TextureFormat depth_format = wgpu::TextureFormat::Depth24Plus;
  render_targets::extent2d extent{};
  render_scene::ptr scene;
};

class project_renderer {
public:
  virtual ~project_renderer() = default;
  virtual bool init(gpu_context &ctx) = 0;
  virtual void update(gpu_context &ctx, uint frame) = 0;
  virtual void render(gpu_context &ctx,
                      const std::function<void(wgpu::RenderPassEncoder &)> &overlay =
                          nullptr) = 0;
  virtual void resize(gpu_context &ctx) = 0;
};

using project_builder = std::function<std::unique_ptr<project_renderer>(gpu_context &)>;

} // namespace lewitt
