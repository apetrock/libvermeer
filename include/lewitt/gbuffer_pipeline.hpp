#pragma once

#include <functional>
#include <iostream>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "lewitt/doables.hpp"
#include "lewitt/frame_context.hpp"
#include "lewitt/nodes.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/resource_handles.hpp"

namespace lewitt {
namespace pipeline {

// Renders g_buffer_renderable children into offscreen position + depth targets.
class gbuffer_pass : public nodes::render_node {
public:
  using drawable = doables::g_buffer_renderable;

  resources::g_buffer_outputs init(wgpu::Device device,
                                   const render_targets::extent2d &extent) {
    _pool.clear();
    _outputs.position.id = _pool.emplace(
        device, resources::sampleable_color_attachment_desc(
                    "gbuffer_position", resources::position_attachment::format()),
        extent);
    _outputs.normal.id = _pool.emplace(
        device, resources::color_attachment_desc("gbuffer_normal",
                                                 resources::normal_attachment::format()),
        extent);
    _outputs.depth.id = _pool.emplace(
        device, render_targets::depth_target_desc(resources::depth_attachment::format()),
        extent);
    _extent = extent;
    return _outputs;
  }

  void resize(wgpu::Device device, const render_targets::extent2d &extent) {
    if (_outputs.position.id == render_targets::invalid_target_id) {
      init(device, extent);
      return;
    }
    _pool.resize(_outputs.position.id, device, extent);
    _pool.resize(_outputs.normal.id, device, extent);
    _pool.resize(_outputs.depth.id, device, extent);
    _extent = extent;
  }

  void add(const drawable::ptr &child) { _children.push_back(child); }
  bool empty() const { return _children.empty(); }

  const resources::g_buffer_outputs &outputs() const { return _outputs; }
  render_targets::target_pool &pool() { return _pool; }
  const render_targets::target_pool &pool() const { return _pool; }

  void render(frame_context &ctx) override {
    execute(ctx.device, nullptr);
  }

  void execute(
      wgpu::Device &device,
      const std::function<void(wgpu::RenderPassEncoder &, wgpu::Device &)> &draw) {
    if (_children.empty()) {
      return;
    }

    const wgpu::TextureFormat color_format = resources::position_attachment::format();
    const wgpu::TextureFormat depth_format = resources::depth_attachment::format();
    for (const auto &child : _children) {
      child->set_texture_format(color_format, depth_format);
    }

    render_targets::frame_attachments attachments{};
    attachments.color_target = _outputs.position.id;
    attachments.color_targets = {_outputs.position.id};
    attachments.depth_target = _outputs.depth.id;

    passes::render(nullptr, device, _pool, attachments,
                   [&](wgpu::RenderPassEncoder &enc, wgpu::Device &dev) {
                     for (const auto &child : _children) {
                       child->draw(enc, dev);
                     }
                     if (draw) {
                       draw(enc, dev);
                     }
                   },
                   false);
  }

private:
  render_targets::target_pool _pool;
  render_targets::extent2d _extent{};
  resources::g_buffer_outputs _outputs{};
  std::vector<drawable::ptr> _children;
};

} // namespace pipeline
} // namespace lewitt
