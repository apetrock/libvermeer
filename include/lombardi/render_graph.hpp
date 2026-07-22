#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "lewitt/frame_context.hpp"
#include "lewitt/gpu_session.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/performance.hpp"
#include "lewitt/present_target.hpp"
#include "lewitt/resource_handles.hpp"
#include "lewitt/texture_target.hpp"
#include "liblombardi/graph_context.hpp"
#include "lombardi/deferred_lighting_node.hpp"
#include "lombardi/forward_mesh_node.hpp"
#include "lombardi/g_buffer_node.hpp"
#include "lombardi/nodes.hpp"
#include "lombardi/ssao_node.hpp"
#include "lombardi/ffmpeg_encode_sink.hpp"

namespace lombardi {

namespace detail {

template <typename TNode>
inline constexpr bool is_graph_owned_render_node_v =
    std::is_same_v<TNode, nodes::g_buffer_node> || std::is_same_v<TNode, nodes::ssao_node> ||
    std::is_same_v<TNode, nodes::deferred_lighting_node> ||
    std::is_same_v<TNode, nodes::forward_mesh_node>;

} // namespace detail

/// Render graph scheduler backed by liblombardi::GraphContext.
/// Topology edges are recorded explicitly via record_edge(); execution order
/// is memoized until mark_topology_dirty() / mark_dirty() is called.
class render_graph : public liblombardi::GraphContext {
public:
  static render_graph create(const lewitt::gpu_context &ctx) {
    render_graph graph;
    graph._extent = ctx.extent;
    graph._present_target =
        lewitt::present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent);
    return graph;
  }

  template <typename TNode, typename... Args>
  std::enable_if_t<!detail::is_graph_owned_render_node_v<TNode>, std::shared_ptr<TNode>>
  construct(Args &&...args) {
    static_assert(std::is_base_of_v<nodes::render_node, TNode>,
                  "render_graph nodes must inherit render_node");
    auto node = create_node<TNode>(std::forward<Args>(args)...);
    return node;
  }

  template <typename TNode, typename = std::enable_if_t<detail::is_graph_owned_render_node_v<TNode>>>
  std::shared_ptr<TNode> construct(wgpu::Device device) {
    if constexpr (std::is_same_v<TNode, nodes::g_buffer_node>) {
      auto node = create_node<nodes::g_buffer_node>();
      const auto targets = allocate_gbuffer(device);
      node->init(device, _pool, targets);
      return node;
    } else if constexpr (std::is_same_v<TNode, nodes::ssao_node>) {
      auto node = create_node<nodes::ssao_node>();
      const auto ao_target = allocate_ssao(device);
      node->init(device, _pool, ao_target);
      return node;
    } else if constexpr (std::is_same_v<TNode, nodes::deferred_lighting_node>) {
      auto node = create_node<nodes::deferred_lighting_node>();
      const auto lit_target = allocate_lit_color(device);
      node->init(device, _pool, lit_target);
      return node;
    } else if constexpr (std::is_same_v<TNode, nodes::forward_mesh_node>) {
      auto node = create_node<nodes::forward_mesh_node>();
      const auto targets = allocate_forward(device);
      node->init(device, _pool, targets);
      return node;
    } else {
      static_assert(detail::is_graph_owned_render_node_v<TNode>,
                    "unsupported graph-owned render node");
      return nullptr;
    }
  }

  void mark_topology_dirty() { mark_dirty(); }

  void set_present_target(const lewitt::present_target &target) { _present_target = target; }

  void resize(wgpu::Device device, const lewitt::render_targets::extent2d &extent) {
    if (!extent.width || !extent.height) {
      return;
    }
    _extent = extent;
    _present_target.extent = extent;
    _pool.resize_all(device, extent);
    for (const auto &node : nodes()) {
      if (auto gbuffer = std::dynamic_pointer_cast<nodes::g_buffer_node>(node)) {
        gbuffer->resize(device);
      } else if (auto ssao = std::dynamic_pointer_cast<nodes::ssao_node>(node)) {
        ssao->resize(device);
      } else if (auto deferred = std::dynamic_pointer_cast<nodes::deferred_lighting_node>(node)) {
        deferred->resize(device);
      } else if (auto forward = std::dynamic_pointer_cast<nodes::forward_mesh_node>(node)) {
        forward->resize(device);
      } else if (auto sink = std::dynamic_pointer_cast<nodes::swapchain_sink>(node)) {
        sink->resize(device);
      } else if (auto ffmpeg = std::dynamic_pointer_cast<nodes::ffmpeg_encode_sink>(node)) {
        ffmpeg->resize(device, _extent);
      }
    }
  }

  void render(lewitt::gpu_context &ctx,
              const std::function<void(wgpu::RenderPassEncoder &)> &overlay = nullptr) {
    LEWITT_PERF_SCOPE_PATH("lombardi::render_graph::render");
    _present_target.swapchain = ctx.swapchain;
    _present_target.format = ctx.swapchain_format;
    _present_target.extent = ctx.extent;

    lewitt::frame_context frame_ctx{ctx.device, ctx.queue, ctx.swapchain, _pool, nullptr, {}};
    for (const auto &node : nodes()) {
      if (auto *render_node = dynamic_cast<nodes::render_node *>(node.get())) {
        render_node->set_frame_context(&frame_ctx);
      }
    }

    if (execution_dirty()) {
      rebuild_schedule();
      clear_execution_dirty();
    }

    for (nodes::render_node *node : _render_schedule) {
      if (!node || node->terminal_sink()) {
        continue;
      }
      node->compute();
    }

    for (nodes::render_node *node : _render_schedule) {
      if (!node || !node->terminal_sink()) {
        continue;
      }
      if (auto *ffmpeg = dynamic_cast<nodes::ffmpeg_encode_sink *>(node)) {
        ffmpeg->encode_frame(frame_ctx, _pool);
      }
    }

    for (nodes::render_node *node : _render_schedule) {
      if (!node || !node->terminal_sink()) {
        continue;
      }
      if (auto sink = dynamic_cast<nodes::swapchain_sink *>(node)) {
        sink->set_present_target(_present_target);
        sink->present(frame_ctx, overlay, _pool);
        return;
      }
    }

    if (overlay) {
      lewitt::render_targets::frame_attachments attachments{};
      attachments.color = lewitt::passes::swapchain_color_attachment(ctx.swapchain);
      lewitt::passes::render(ctx.swapchain, ctx.device, attachments,
                             [&](wgpu::RenderPassEncoder &enc, wgpu::Device &) { overlay(enc); },
                             true);
    }
  }

  lewitt::render_targets::target_pool &pool() { return _pool; }
  const lewitt::render_targets::target_pool &pool() const { return _pool; }

  lewitt::render_targets::target_id color_target(const std::string &name, wgpu::Device device,
                                                 wgpu::TextureFormat format =
                                                     wgpu::TextureFormat::BGRA8Unorm) {
    if (_named_targets.count(name)) {
      return _named_targets.at(name);
    }
    const auto id = _pool.emplace(
        device, lewitt::resources::sampleable_color_attachment_desc(name.c_str(), format), _extent);
    _named_targets.emplace(name, id);
    return id;
  }

  void record_edge(nodes::render_node &from, nodes::render_node &to) {
    liblombardi::GraphContext::record_edge(from, to);
  }

protected:
  void rebuild_schedule() override {
    liblombardi::GraphContext::rebuild_schedule();

    _render_schedule.clear();
    _render_schedule.reserve(nodes().size());

    for (liblombardi::NodeBase *node : execution_schedule()) {
      auto *render_node = dynamic_cast<nodes::render_node *>(node);
      if (!render_node || render_node->terminal_sink()) {
        continue;
      }
      _render_schedule.push_back(render_node);
    }

    for (liblombardi::NodeBase *node : execution_schedule()) {
      auto *render_node = dynamic_cast<nodes::render_node *>(node);
      if (!render_node || !render_node->terminal_sink()) {
        continue;
      }
      _render_schedule.push_back(render_node);
    }

    for (const auto &node : nodes()) {
      auto *render_node = dynamic_cast<nodes::render_node *>(node.get());
      if (!render_node) {
        continue;
      }
      if (std::find(_render_schedule.begin(), _render_schedule.end(), render_node) ==
          _render_schedule.end()) {
        _render_schedule.push_back(render_node);
      }
    }
  }

private:
  lewitt::resources::g_buffer_outputs allocate_gbuffer(wgpu::Device device) {
    lewitt::resources::g_buffer_outputs out{};
    out.position.id = _pool.emplace(
        device,
        lewitt::resources::sampleable_color_attachment_desc(
            "gbuffer_position", lewitt::resources::position_attachment::format()),
        _extent);
    out.normal.id = _pool.emplace(
        device,
        lewitt::resources::sampleable_color_attachment_desc(
            "gbuffer_normal", lewitt::resources::normal_attachment::format()),
        _extent);
    out.albedo_spec.id = _pool.emplace(
        device,
        lewitt::resources::sampleable_color_attachment_desc(
            "gbuffer_albedo", lewitt::resources::albedo_spec_attachment::format()),
        _extent);
    out.depth.id = _pool.emplace(
        device,
        lewitt::render_targets::depth_target_desc(lewitt::resources::depth_attachment::format()),
        _extent);
    return out;
  }

  lewitt::resources::handle<lewitt::resources::ssao_attachment> allocate_ssao(wgpu::Device device) {
    lewitt::resources::handle<lewitt::resources::ssao_attachment> out{};
    out.id = _pool.emplace(
        device,
        lewitt::resources::sampleable_color_attachment_desc(
            "ssao", lewitt::resources::ssao_attachment::format()),
        _extent);
    return out;
  }

  lewitt::resources::handle<lewitt::resources::lit_color_attachment>
  allocate_lit_color(wgpu::Device device) {
    lewitt::resources::handle<lewitt::resources::lit_color_attachment> out{};
    out.id = _pool.emplace(
        device,
        lewitt::resources::sampleable_color_attachment_desc(
            "lit_color", lewitt::resources::lit_color_attachment::format()),
        _extent);
    return out;
  }

  nodes::forward_outputs allocate_forward(wgpu::Device device) {
    nodes::forward_outputs out{};
    out.color.id = _pool.emplace(
        device,
        lewitt::resources::sampleable_color_attachment_desc(
            "forward_color", lewitt::resources::lit_color_attachment::format()),
        _extent);
    out.depth.id = _pool.emplace(
        device,
        lewitt::render_targets::depth_target_desc(lewitt::resources::depth_attachment::format()),
        _extent);
    return out;
  }

  lewitt::render_targets::target_pool _pool;
  lewitt::render_targets::extent2d _extent{};
  lewitt::present_target _present_target{};
  std::unordered_map<std::string, lewitt::render_targets::target_id> _named_targets;
  std::vector<nodes::render_node *> _render_schedule;
};

} // namespace lombardi
