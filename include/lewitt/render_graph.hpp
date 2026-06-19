#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "lewitt/deferred_lighting_node.hpp"
#include "lewitt/frame_context.hpp"
#include "lewitt/g_buffer_node.hpp"
#include "lewitt/gpu_session.hpp"
#include "lewitt/nodes.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/present_target.hpp"
#include "lewitt/resource_handles.hpp"
#include "lewitt/ssao_node.hpp"
#include "lewitt/texture_target.hpp"

namespace lewitt {

class render_graph {
public:
  static render_graph create(const gpu_context &ctx) {
    render_graph graph;
    graph._extent = ctx.extent;
    graph._present_target =
        present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent);
    return graph;
  }

  template <typename TNode, typename... Args>
  std::shared_ptr<TNode> construct(Args &&...args) {
    static_assert(std::is_base_of_v<nodes::render_node, TNode>,
                  "render_graph nodes must inherit render_node");
    auto node = std::make_shared<TNode>(std::forward<Args>(args)...);
    _nodes.push_back(node);
    mark_topology_dirty();
    return node;
  }

  void mark_topology_dirty() { _execution_order_dirty = true; }

  resources::g_buffer_outputs allocate_gbuffer(wgpu::Device device) {
    resources::g_buffer_outputs out{};
    out.position.id = _pool.emplace(
        device,
        resources::sampleable_color_attachment_desc(
            "gbuffer_position", resources::position_attachment::format()),
        _extent);
    out.normal.id = _pool.emplace(
        device,
        resources::sampleable_color_attachment_desc("gbuffer_normal",
                                                     resources::normal_attachment::format()),
        _extent);
    out.albedo_spec.id = _pool.emplace(
        device,
        resources::sampleable_color_attachment_desc(
            "gbuffer_albedo", resources::albedo_spec_attachment::format()),
        _extent);
    out.depth.id = _pool.emplace(
        device, render_targets::depth_target_desc(resources::depth_attachment::format()),
        _extent);
    return out;
  }

  resources::handle<resources::ssao_attachment> allocate_ssao(wgpu::Device device) {
    resources::handle<resources::ssao_attachment> out{};
    out.id = _pool.emplace(
        device,
        resources::sampleable_color_attachment_desc("ssao", resources::ssao_attachment::format()),
        _extent);
    return out;
  }

  resources::handle<resources::lit_color_attachment> allocate_lit_color(wgpu::Device device) {
    resources::handle<resources::lit_color_attachment> out{};
    out.id = _pool.emplace(
        device,
        resources::sampleable_color_attachment_desc("lit_color",
                                                    resources::lit_color_attachment::format()),
        _extent);
    return out;
  }

  render_targets::target_id color_target(const std::string &name, wgpu::Device device,
                                         wgpu::TextureFormat format =
                                             wgpu::TextureFormat::BGRA8Unorm) {
    if (_named_targets.count(name)) {
      return _named_targets.at(name);
    }
    const auto id = _pool.emplace(
        device,
        resources::sampleable_color_attachment_desc(name.c_str(), format),
        _extent);
    _named_targets.emplace(name, id);
    return id;
  }

  void set_present_target(const present_target &target) { _present_target = target; }

  void resize(wgpu::Device device, const render_targets::extent2d &extent) {
    if (!extent.width || !extent.height) {
      return;
    }
    _extent = extent;
    _present_target.extent = extent;
    _pool.resize_all(device, extent);
    for (const auto &node : _nodes) {
      if (auto gbuffer = std::dynamic_pointer_cast<nodes::g_buffer_node>(node)) {
        gbuffer->resize(device);
      } else if (auto ssao = std::dynamic_pointer_cast<nodes::ssao_node>(node)) {
        ssao->resize(device);
      } else if (auto deferred =
                     std::dynamic_pointer_cast<nodes::deferred_lighting_node>(node)) {
        deferred->resize(device);
      } else if (auto sink = std::dynamic_pointer_cast<nodes::swapchain_sink>(node)) {
        sink->resize(device);
      }
    }
  }

  void render(gpu_context &ctx,
              const std::function<void(wgpu::RenderPassEncoder &)> &overlay = nullptr) {
    _present_target.swapchain = ctx.swapchain;
    _present_target.format = ctx.swapchain_format;
    _present_target.extent = ctx.extent;

    frame_context frame_ctx{ctx.device, ctx.queue, ctx.swapchain, _pool, nullptr, {}};

    const auto &ordered = execution_order();

    for (const auto &node : ordered) {
      if (!node || node->terminal_sink()) {
        continue;
      }
      node->render(frame_ctx);
    }

    for (const auto &node : ordered) {
      auto sink = std::dynamic_pointer_cast<nodes::swapchain_sink>(node);
      if (!sink) {
        continue;
      }
      sink->set_present_target(_present_target);
      sink->present(frame_ctx, overlay, _pool);
      return;
    }

    if (overlay) {
      render_targets::frame_attachments attachments{};
      attachments.color = passes::swapchain_color_attachment(ctx.swapchain);
      passes::render(ctx.swapchain, ctx.device, attachments,
                     [&](wgpu::RenderPassEncoder &enc, wgpu::Device &) { overlay(enc); },
                     true);
    }
  }

  render_targets::target_pool &pool() { return _pool; }
  const render_targets::target_pool &pool() const { return _pool; }

private:
  const std::vector<std::shared_ptr<nodes::render_node>> &execution_order() {
    if (_execution_order_dirty) {
      _cached_execution_order = compute_execution_order();
      _execution_order_dirty = false;
    }
    return _cached_execution_order;
  }

  std::vector<std::shared_ptr<nodes::render_node>> compute_execution_order() const {
    std::unordered_map<render_targets::target_id, std::shared_ptr<nodes::render_node>> writers;
    std::vector<std::shared_ptr<nodes::render_node>> producers;
    std::vector<std::shared_ptr<nodes::render_node>> sinks;

    for (const auto &node : _nodes) {
      if (!node) {
        continue;
      }
      if (node->terminal_sink()) {
        sinks.push_back(node);
        continue;
      }
      producers.push_back(node);
      for (const auto &use : node->resource_uses()) {
        if (use.kind == nodes::resource_use_kind::write &&
            use.resource != render_targets::invalid_target_id) {
          writers[use.resource] = node;
        }
      }
    }

    std::unordered_map<const nodes::render_node *, int> indegree;
    std::unordered_map<const nodes::render_node *, std::vector<const nodes::render_node *>>
        edges;
    for (const auto &node : producers) {
      indegree[node.get()] = 0;
    }

    for (const auto &node : producers) {
      for (const auto &use : node->resource_uses()) {
        if (use.kind != nodes::resource_use_kind::read ||
            use.resource == render_targets::invalid_target_id) {
          continue;
        }
        const auto writer_it = writers.find(use.resource);
        if (writer_it == writers.end() || writer_it->second.get() == node.get()) {
          continue;
        }
        const auto *writer = writer_it->second.get();
        edges[writer].push_back(node.get());
        indegree[node.get()] += 1;
      }
    }

    std::vector<std::shared_ptr<nodes::render_node>> ordered;
    ordered.reserve(_nodes.size());
    std::vector<const nodes::render_node *> ready;
    for (const auto &node : producers) {
      if (indegree[node.get()] == 0) {
        ready.push_back(node.get());
      }
    }

    while (!ready.empty()) {
      const nodes::render_node *current = ready.back();
      ready.pop_back();
      for (const auto &node : producers) {
        if (node.get() == current) {
          ordered.push_back(node);
          break;
        }
      }
      for (const auto *next : edges[current]) {
        indegree[next] -= 1;
        if (indegree[next] == 0) {
          ready.push_back(next);
        }
      }
    }

    if (ordered.size() != producers.size()) {
      for (const auto &node : producers) {
        if (std::find_if(ordered.begin(), ordered.end(),
                         [&](const auto &existing) { return existing.get() == node.get(); }) ==
            ordered.end()) {
          ordered.push_back(node);
        }
      }
    }

    for (const auto &sink : sinks) {
      ordered.push_back(sink);
    }
    return ordered;
  }

  render_targets::target_pool _pool;
  render_targets::extent2d _extent{};
  present_target _present_target{};
  std::unordered_map<std::string, render_targets::target_id> _named_targets;
  std::vector<std::shared_ptr<nodes::render_node>> _nodes;
  std::vector<std::shared_ptr<nodes::render_node>> _cached_execution_order;
  bool _execution_order_dirty = true;
};

} // namespace lewitt
