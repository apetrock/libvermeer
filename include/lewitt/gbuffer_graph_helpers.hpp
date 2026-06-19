#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "lewitt/debug_line_buffer.hpp"
#include "lewitt/deferred_lighting_node.hpp"
#include "lewitt/g_buffer_node.hpp"
#include "lewitt/gpu_session.hpp"
#include "lewitt/mesh_buffer.hpp"
#include "lewitt/nodes.hpp"
#include "lewitt/present_target.hpp"
#include "lewitt/render_graph.hpp"
#include "lewitt/resource_handles.hpp"
#include "lewitt/ssao_node.hpp"

namespace lewitt {

enum class render_pipeline_mode { gbuffer_visualizer, ssao_debug, deferred_lit };

struct gbuffer_visualizer_pipeline {
  render_graph graph;
  resources::g_buffer_outputs gbuffer_targets{};
  std::shared_ptr<nodes::g_buffer_node> gbuffer;
  std::shared_ptr<nodes::swapchain_sink> sink;
  nodes::sink_display_mode mode = nodes::sink_display_mode::normal;

  static gbuffer_visualizer_pipeline
  create(gpu_context &ctx, const std::vector<std::weak_ptr<mesh_buffer>> &meshes,
         const std::vector<std::weak_ptr<debug_line_buffer>> &debug_lines = {},
         nodes::sink_display_mode display_mode = nodes::sink_display_mode::normal) {
    gbuffer_visualizer_pipeline pipeline;
    pipeline.mode = display_mode;
    pipeline.graph = render_graph::create(ctx);
    pipeline.graph.set_present_target(
        present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent));

    pipeline.gbuffer_targets = pipeline.graph.allocate_gbuffer(ctx.device);
    pipeline.gbuffer = pipeline.graph.construct<nodes::g_buffer_node>();
    pipeline.gbuffer->init(ctx.device, pipeline.graph.pool(), pipeline.gbuffer_targets,
                           ctx.scene->camera_uniform_binding());
    pipeline.gbuffer->set_input(nodes::g_buffer_node::input::meshes, meshes);
    pipeline.gbuffer->set_input(nodes::g_buffer_node::input::debug_lines, debug_lines);

    pipeline.sink = pipeline.graph.construct<nodes::swapchain_sink>();
    pipeline.sink->init(ctx.device, present_target::from_context(
                                          ctx.swapchain, ctx.swapchain_format, ctx.extent));
    pipeline.sink->set_mode(display_mode);
    pipeline.wire_sink_source();
    pipeline.graph.mark_topology_dirty();
    return pipeline;
  }

  void set_meshes(const std::vector<std::weak_ptr<mesh_buffer>> &meshes) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::meshes, meshes);
    }
  }

  void set_debug_lines(const std::vector<std::weak_ptr<debug_line_buffer>> &debug_lines) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::debug_lines, debug_lines);
    }
  }

  void rebind_pool() {
    if (gbuffer) {
      gbuffer->set_pool(graph.pool());
    }
  }

  void set_mode(nodes::sink_display_mode display_mode) {
    mode = display_mode;
    wire_sink_source();
    graph.mark_topology_dirty();
  }

  void wire_sink_source() {
    if (!gbuffer || !sink) {
      return;
    }

    switch (mode) {
    case nodes::sink_display_mode::position: {
      resources::texture_input<resources::position_sampled> input{};
      resources::wire(gbuffer->outputs().position, input.source);
      sink->set_input(nodes::swapchain_sink::input_port::source, input);
      break;
    }
    case nodes::sink_display_mode::normal: {
      resources::texture_input<resources::normal_sampled> input{};
      resources::wire(gbuffer->outputs().normal, input.source);
      sink->set_input(nodes::swapchain_sink::input_port::source, input);
      break;
    }
    case nodes::sink_display_mode::color:
    case nodes::sink_display_mode::depth:
      break;
    default:
      break;
    }
  }

  void resize(gpu_context &ctx) {
    if (!ctx.extent.width || !ctx.extent.height) {
      return;
    }
    if (sink) {
      sink->init(ctx.device, present_target::from_context(
                                 ctx.swapchain, ctx.swapchain_format, ctx.extent));
    }
    graph.set_present_target(
        present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent));
    graph.resize(ctx.device, ctx.extent);
    wire_sink_source();
    graph.mark_topology_dirty();
  }

  void render(gpu_context &ctx,
              const std::function<void(wgpu::RenderPassEncoder &)> &overlay = nullptr) {
    graph.render(ctx, overlay);
  }
};

struct ssao_deferred_pipeline {
  render_graph graph;
  resources::g_buffer_outputs gbuffer_targets{};
  resources::handle<resources::ssao_attachment> ssao_target{};
  resources::handle<resources::lit_color_attachment> lit_target{};
  std::shared_ptr<nodes::g_buffer_node> gbuffer;
  std::shared_ptr<nodes::ssao_node> ssao;
  std::shared_ptr<nodes::deferred_lighting_node> deferred;
  std::shared_ptr<nodes::swapchain_sink> sink;
  render_pipeline_mode mode = render_pipeline_mode::deferred_lit;

  static ssao_deferred_pipeline
  create(gpu_context &ctx, const std::vector<std::weak_ptr<mesh_buffer>> &meshes,
         const std::vector<std::weak_ptr<debug_line_buffer>> &debug_lines = {},
         render_pipeline_mode pipeline_mode = render_pipeline_mode::deferred_lit) {
    ssao_deferred_pipeline pipeline;
    pipeline.mode = pipeline_mode;
    pipeline.graph = render_graph::create(ctx);
    pipeline.graph.set_present_target(
        present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent));

    pipeline.gbuffer_targets = pipeline.graph.allocate_gbuffer(ctx.device);
    pipeline.ssao_target = pipeline.graph.allocate_ssao(ctx.device);
    pipeline.lit_target = pipeline.graph.allocate_lit_color(ctx.device);

    pipeline.gbuffer = pipeline.graph.construct<nodes::g_buffer_node>();
    pipeline.gbuffer->init(ctx.device, pipeline.graph.pool(), pipeline.gbuffer_targets,
                           ctx.scene->camera_uniform_binding());
    pipeline.gbuffer->set_input(nodes::g_buffer_node::input::meshes, meshes);
    pipeline.gbuffer->set_input(nodes::g_buffer_node::input::debug_lines, debug_lines);

    pipeline.ssao = pipeline.graph.construct<nodes::ssao_node>();
    pipeline.ssao->init(ctx.device, pipeline.graph.pool(), pipeline.ssao_target,
                        ctx.scene->camera_uniform_binding());

    pipeline.deferred = pipeline.graph.construct<nodes::deferred_lighting_node>();
    pipeline.deferred->init(ctx.device, pipeline.graph.pool(), pipeline.lit_target);

    pipeline.sink = pipeline.graph.construct<nodes::swapchain_sink>();
    pipeline.sink->init(ctx.device, present_target::from_context(
                                          ctx.swapchain, ctx.swapchain_format, ctx.extent));

    pipeline.wire_graph();
    pipeline.graph.mark_topology_dirty();
    return pipeline;
  }

  void set_meshes(const std::vector<std::weak_ptr<mesh_buffer>> &meshes) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::meshes, meshes);
    }
  }

  void set_debug_lines(const std::vector<std::weak_ptr<debug_line_buffer>> &debug_lines) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::debug_lines, debug_lines);
    }
  }

  void rebind_pool() {
    if (gbuffer) {
      gbuffer->set_pool(graph.pool());
    }
    if (ssao) {
      ssao->set_pool(graph.pool());
    }
    if (deferred) {
      deferred->set_pool(graph.pool());
    }
  }

  void set_mode(render_pipeline_mode pipeline_mode) {
    mode = pipeline_mode;
    wire_graph();
    graph.mark_topology_dirty();
  }

  void wire_graph() {
    if (!gbuffer || !ssao || !deferred || !sink) {
      return;
    }

    resources::texture_input<resources::position_sampled> position{};
    resources::texture_input<resources::normal_sampled> normal{};
    resources::texture_input<resources::albedo_spec_sampled> albedo{};
    resources::texture_input<resources::ssao_sampled> ssao_input{};
    resources::wire(gbuffer->outputs().position, position.source);
    resources::wire(gbuffer->outputs().normal, normal.source);
    resources::wire(gbuffer->outputs().albedo_spec, albedo.source);
    resources::wire(ssao_target, ssao_input.source);

    ssao->set_input(nodes::ssao_node::input::position, position);
    ssao->set_input(nodes::ssao_node::input::normal, normal);

    deferred->set_input(nodes::deferred_lighting_node::input::position, position);
    deferred->set_input(nodes::deferred_lighting_node::input::normal, normal);
    deferred->set_input(nodes::deferred_lighting_node::input::albedo_spec, albedo);
    deferred->set_input(nodes::deferred_lighting_node::input::ssao, ssao_input);

    switch (mode) {
    case render_pipeline_mode::gbuffer_visualizer: {
      resources::texture_input<resources::normal_sampled> normal_view{};
      resources::wire(gbuffer->outputs().normal, normal_view.source);
      sink->set_mode(nodes::sink_display_mode::normal);
      sink->set_input(nodes::swapchain_sink::input_port::source, normal_view);
      break;
    }
    case render_pipeline_mode::ssao_debug: {
      resources::texture_input<resources::ssao_sampled> ssao_view{};
      resources::wire(ssao_target, ssao_view.source);
      sink->set_mode(nodes::sink_display_mode::ssao);
      sink->set_input(nodes::swapchain_sink::input_port::source, ssao_view);
      break;
    }
    case render_pipeline_mode::deferred_lit: {
      resources::texture_input<resources::color_sampled> lit_view{};
      resources::wire(lit_target, lit_view.source);
      sink->set_mode(nodes::sink_display_mode::color);
      sink->set_input(nodes::swapchain_sink::input_port::source, lit_view);
      break;
    }
    }
  }

  void resize(gpu_context &ctx) {
    if (!ctx.extent.width || !ctx.extent.height) {
      return;
    }
    if (sink) {
      sink->init(ctx.device, present_target::from_context(
                                 ctx.swapchain, ctx.swapchain_format, ctx.extent));
    }
    graph.set_present_target(
        present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent));
    graph.resize(ctx.device, ctx.extent);
    wire_graph();
    graph.mark_topology_dirty();
  }

  void render(gpu_context &ctx,
              const std::function<void(wgpu::RenderPassEncoder &)> &overlay = nullptr) {
    graph.render(ctx, overlay);
  }
};

inline gbuffer_visualizer_pipeline
make_gbuffer_visualizer_pipeline(gpu_context &ctx,
                               const std::vector<std::weak_ptr<mesh_buffer>> &meshes,
                               const std::vector<std::weak_ptr<debug_line_buffer>> &debug_lines =
                                   {},
                               nodes::sink_display_mode mode = nodes::sink_display_mode::normal) {
  return gbuffer_visualizer_pipeline::create(ctx, meshes, debug_lines, mode);
}

inline ssao_deferred_pipeline
make_ssao_deferred_pipeline(gpu_context &ctx,
                          const std::vector<std::weak_ptr<mesh_buffer>> &meshes,
                          const std::vector<std::weak_ptr<debug_line_buffer>> &debug_lines = {},
                          render_pipeline_mode mode = render_pipeline_mode::deferred_lit) {
  return ssao_deferred_pipeline::create(ctx, meshes, debug_lines, mode);
}

} // namespace lewitt
