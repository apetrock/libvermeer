#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "lewitt/debug_line_buffer.hpp"
#include "lewitt/debug_sphere_buffer.hpp"
#include "lewitt/debug_torus_buffer.hpp"
#include "lewitt/gpu_session.hpp"
#include "lewitt/mesh_buffer.hpp"
#include "lewitt/present_target.hpp"
#include "lewitt/resource_handles.hpp"
#include "lombardi/deferred_lighting_node.hpp"
#include "lombardi/forward_mesh_node.hpp"
#include "lombardi/ffmpeg_record_config.hpp"
#include "lombardi/ffmpeg_encode_sink.hpp"
#include "lombardi/g_buffer_node.hpp"
#include "lombardi/nodes.hpp"
#include "lombardi/render_graph.hpp"
#include "lombardi/ssao_node.hpp"

namespace lombardi {

enum class render_pipeline_mode { gbuffer_visualizer, ssao_debug, deferred_lit };

struct gbuffer_visualizer_pipeline {
  render_graph graph;
  std::shared_ptr<nodes::g_buffer_node> gbuffer;
  std::shared_ptr<nodes::swapchain_sink> sink;
  nodes::sink_display_mode mode = nodes::sink_display_mode::normal;

  static gbuffer_visualizer_pipeline
  create(lewitt::gpu_context &ctx, const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes,
         const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines = {},
         nodes::sink_display_mode display_mode = nodes::sink_display_mode::normal) {
    gbuffer_visualizer_pipeline pipeline;
    pipeline.mode = display_mode;
    pipeline.graph = render_graph::create(ctx);
    pipeline.graph.set_present_target(
        lewitt::present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent));

    pipeline.gbuffer = pipeline.graph.construct<nodes::g_buffer_node>(ctx.device);
    pipeline.gbuffer->set_input(nodes::g_buffer_node::input::camera,
                                ctx.scene->camera_uniform_binding());
    pipeline.gbuffer->set_input(nodes::g_buffer_node::input::meshes, meshes);
    pipeline.gbuffer->set_input(nodes::g_buffer_node::input::debug_lines, debug_lines);

    pipeline.sink = pipeline.graph.construct<nodes::swapchain_sink>();
    pipeline.sink->init(ctx.device, lewitt::present_target::from_context(
                                          ctx.swapchain, ctx.swapchain_format, ctx.extent));
    pipeline.sink->set_mode(display_mode);
    pipeline.wire_sink_source();
    pipeline.graph.mark_topology_dirty();
    return pipeline;
  }

  void set_meshes(const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::meshes, meshes);
    }
  }

  void set_debug_lines(const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::debug_lines, debug_lines);
    }
  }

  void set_debug_spheres(const std::vector<std::weak_ptr<lewitt::debug_sphere_buffer>> &debug_spheres) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::debug_spheres, debug_spheres);
    }
  }

  void set_debug_tori(const std::vector<std::weak_ptr<lewitt::debug_torus_buffer>> &debug_tori) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::debug_tori, debug_tori);
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

    graph.clear_edges();
    graph.record_edge(*gbuffer, *sink);

    switch (mode) {
    case nodes::sink_display_mode::position: {
      lewitt::resources::texture_input<lewitt::resources::position_sampled> input{};
      lewitt::resources::wire(gbuffer->outputs().position, input.source);
      sink->set_input(nodes::swapchain_sink::input_port::source, input);
      break;
    }
    case nodes::sink_display_mode::normal: {
      lewitt::resources::texture_input<lewitt::resources::normal_sampled> input{};
      lewitt::resources::wire(gbuffer->outputs().normal, input.source);
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

  void resize(lewitt::gpu_context &ctx) {
    if (!ctx.extent.width || !ctx.extent.height) {
      return;
    }
    if (sink) {
      sink->init(ctx.device, lewitt::present_target::from_context(
                                 ctx.swapchain, ctx.swapchain_format, ctx.extent));
    }
    graph.set_present_target(
        lewitt::present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent));
    graph.resize(ctx.device, ctx.extent);
    wire_sink_source();
    graph.mark_topology_dirty();
  }

  void render(lewitt::gpu_context &ctx,
              const std::function<void(wgpu::RenderPassEncoder &)> &overlay = nullptr) {
    graph.render(ctx, overlay);
  }
};

struct ssao_deferred_pipeline {
  render_graph graph;
  std::shared_ptr<nodes::g_buffer_node> gbuffer;
  std::shared_ptr<nodes::ssao_node> ssao;
  std::shared_ptr<nodes::deferred_lighting_node> deferred;
  std::shared_ptr<nodes::swapchain_sink> sink;
  std::shared_ptr<nodes::ffmpeg_encode_sink> ffmpeg;
  render_pipeline_mode mode = render_pipeline_mode::deferred_lit;
  ffmpeg_record_config record{};

  static ssao_deferred_pipeline
  create(lewitt::gpu_context &ctx, const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes,
         const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines = {},
         render_pipeline_mode pipeline_mode = render_pipeline_mode::deferred_lit,
         const ffmpeg_record_config &record_config = {}) {
    ssao_deferred_pipeline pipeline;
    pipeline.mode = pipeline_mode;
    pipeline.record = record_config;
    pipeline.graph = render_graph::create(ctx);
    pipeline.graph.set_present_target(
        lewitt::present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent));

    pipeline.gbuffer = pipeline.graph.construct<nodes::g_buffer_node>(ctx.device);
    pipeline.gbuffer->set_input(nodes::g_buffer_node::input::camera,
                                ctx.scene->camera_uniform_binding());
    pipeline.gbuffer->set_input(nodes::g_buffer_node::input::meshes, meshes);
    pipeline.gbuffer->set_input(nodes::g_buffer_node::input::debug_lines, debug_lines);

    pipeline.ssao = pipeline.graph.construct<nodes::ssao_node>(ctx.device);
    pipeline.ssao->set_input(nodes::ssao_node::input::camera, ctx.scene->camera_uniform_binding());

    pipeline.deferred = pipeline.graph.construct<nodes::deferred_lighting_node>(ctx.device);

    pipeline.sink = pipeline.graph.construct<nodes::swapchain_sink>();
    pipeline.sink->init(ctx.device, lewitt::present_target::from_context(
                                          ctx.swapchain, ctx.swapchain_format, ctx.extent));

#if !defined(_WIN32)
    if (record_config.enabled) {
      pipeline.ffmpeg = pipeline.graph.construct<nodes::ffmpeg_encode_sink>();
      pipeline.ffmpeg->init(ctx.device, ctx.extent, record_config);
    }
#endif

    pipeline.wire_graph();
    pipeline.graph.mark_topology_dirty();
    return pipeline;
  }

  void set_meshes(const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::meshes, meshes);
    }
  }

  void set_debug_lines(const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::debug_lines, debug_lines);
    }
  }

  void set_debug_spheres(const std::vector<std::weak_ptr<lewitt::debug_sphere_buffer>> &debug_spheres) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::debug_spheres, debug_spheres);
    }
  }

  void set_debug_tori(const std::vector<std::weak_ptr<lewitt::debug_torus_buffer>> &debug_tori) {
    if (gbuffer) {
      gbuffer->set_input(nodes::g_buffer_node::input::debug_tori, debug_tori);
    }
  }

  void request_record_frame() {
    if (ffmpeg)
      ffmpeg->request_encode();
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

    graph.clear_edges();

    lewitt::resources::texture_input<lewitt::resources::position_sampled> position{};
    lewitt::resources::texture_input<lewitt::resources::normal_sampled> normal{};
    lewitt::resources::texture_input<lewitt::resources::albedo_spec_sampled> albedo{};
    lewitt::resources::texture_input<lewitt::resources::ssao_sampled> ssao_input{};
    lewitt::resources::wire(gbuffer->outputs().position, position.source);
    lewitt::resources::wire(gbuffer->outputs().normal, normal.source);
    lewitt::resources::wire(gbuffer->outputs().albedo_spec, albedo.source);
    lewitt::resources::wire(ssao->output(), ssao_input.source);

    ssao->set_input(nodes::ssao_node::input::position, position);
    ssao->set_input(nodes::ssao_node::input::normal, normal);

    graph.record_edge(*gbuffer, *ssao);
    graph.record_edge(*gbuffer, *deferred);
    graph.record_edge(*ssao, *deferred);

    deferred->set_input(nodes::deferred_lighting_node::input::position, position);
    deferred->set_input(nodes::deferred_lighting_node::input::normal, normal);
    deferred->set_input(nodes::deferred_lighting_node::input::albedo_spec, albedo);
    deferred->set_input(nodes::deferred_lighting_node::input::ssao, ssao_input);

    switch (mode) {
    case render_pipeline_mode::gbuffer_visualizer: {
      lewitt::resources::texture_input<lewitt::resources::normal_sampled> normal_view{};
      lewitt::resources::wire(gbuffer->outputs().normal, normal_view.source);
      graph.record_edge(*gbuffer, *sink);
      sink->set_mode(nodes::sink_display_mode::normal);
      sink->set_input(nodes::swapchain_sink::input_port::source, normal_view);
      break;
    }
    case render_pipeline_mode::ssao_debug: {
      lewitt::resources::texture_input<lewitt::resources::ssao_sampled> ssao_view{};
      lewitt::resources::wire(ssao->output(), ssao_view.source);
      graph.record_edge(*ssao, *sink);
      sink->set_mode(nodes::sink_display_mode::ssao);
      sink->set_input(nodes::swapchain_sink::input_port::source, ssao_view);
      break;
    }
    case render_pipeline_mode::deferred_lit: {
      lewitt::resources::texture_input<lewitt::resources::color_sampled> lit_for_sink{};
      lewitt::resources::wire(deferred->output(), lit_for_sink.source);
      graph.record_edge(*deferred, *sink);
      sink->set_mode(nodes::sink_display_mode::color);
      sink->set_input(nodes::swapchain_sink::input_port::source, lit_for_sink);
      if (ffmpeg) {
        lewitt::resources::texture_input<lewitt::resources::color_sampled> lit_for_ffmpeg{};
        lewitt::resources::wire(deferred->output(), lit_for_ffmpeg.source);
        graph.record_edge(*deferred, *ffmpeg);
        ffmpeg->set_input(nodes::ffmpeg_encode_sink::input_port::source, lit_for_ffmpeg);
      }
      break;
    }
    }
  }

  void resize(lewitt::gpu_context &ctx) {
    if (!ctx.extent.width || !ctx.extent.height) {
      return;
    }
    if (sink) {
      sink->init(ctx.device, lewitt::present_target::from_context(
                                 ctx.swapchain, ctx.swapchain_format, ctx.extent));
    }
    if (ffmpeg) {
      ffmpeg->resize(ctx.device, ctx.extent);
    }
    graph.set_present_target(
        lewitt::present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent));
    graph.resize(ctx.device, ctx.extent);
    wire_graph();
    graph.mark_topology_dirty();
  }

  void render(lewitt::gpu_context &ctx,
              const std::function<void(wgpu::RenderPassEncoder &)> &overlay = nullptr) {
    graph.render(ctx, overlay);
  }
};

struct forward_pipeline {
  render_graph graph;
  std::shared_ptr<nodes::forward_mesh_node> forward;
  std::shared_ptr<nodes::swapchain_sink> sink;
  std::shared_ptr<nodes::ffmpeg_encode_sink> ffmpeg;
  ffmpeg_record_config record{};

  static forward_pipeline
  create(lewitt::gpu_context &ctx, const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes,
         const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines = {},
         const ffmpeg_record_config &record_config = {}) {
    forward_pipeline pipeline;
    pipeline.record = record_config;
    pipeline.graph = render_graph::create(ctx);
    pipeline.graph.set_present_target(
        lewitt::present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent));

    pipeline.forward = pipeline.graph.construct<nodes::forward_mesh_node>(ctx.device);
    pipeline.forward->set_input(nodes::forward_mesh_node::input::camera,
                                ctx.scene->camera_uniform_binding());
    pipeline.forward->set_input(nodes::forward_mesh_node::input::lighting,
                                ctx.scene->lighting_uniform_binding());
    pipeline.forward->set_input(nodes::forward_mesh_node::input::meshes, meshes);
    pipeline.forward->set_input(nodes::forward_mesh_node::input::debug_lines, debug_lines);

    pipeline.sink = pipeline.graph.construct<nodes::swapchain_sink>();
    pipeline.sink->init(ctx.device, lewitt::present_target::from_context(
                                          ctx.swapchain, ctx.swapchain_format, ctx.extent));

#if !defined(_WIN32)
    if (record_config.enabled) {
      pipeline.ffmpeg = pipeline.graph.construct<nodes::ffmpeg_encode_sink>();
      pipeline.ffmpeg->init(ctx.device, ctx.extent, record_config);
    }
#endif

    pipeline.wire_graph();
    pipeline.graph.mark_topology_dirty();
    return pipeline;
  }

  void set_meshes(const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes) {
    if (forward) {
      forward->set_input(nodes::forward_mesh_node::input::meshes, meshes);
    }
  }

  void set_debug_lines(const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines) {
    if (forward) {
      forward->set_input(nodes::forward_mesh_node::input::debug_lines, debug_lines);
    }
  }

  void set_debug_spheres(const std::vector<std::weak_ptr<lewitt::debug_sphere_buffer>> &debug_spheres) {
    if (forward) {
      forward->set_input(nodes::forward_mesh_node::input::debug_spheres, debug_spheres);
    }
  }

  void set_debug_tori(const std::vector<std::weak_ptr<lewitt::debug_torus_buffer>> &debug_tori) {
    if (forward) {
      forward->set_input(nodes::forward_mesh_node::input::debug_tori, debug_tori);
    }
  }

  void request_record_frame() {
    if (ffmpeg)
      ffmpeg->request_encode();
  }

  void rebind_pool() {
    if (forward) {
      forward->set_pool(graph.pool());
    }
  }

  void wire_graph() {
    if (!forward || !sink) {
      return;
    }

    graph.clear_edges();

    lewitt::resources::texture_input<lewitt::resources::color_sampled> color_for_sink{};
    lewitt::resources::wire(forward->outputs().color, color_for_sink.source);
    graph.record_edge(*forward, *sink);
    sink->set_mode(nodes::sink_display_mode::color);
    sink->set_input(nodes::swapchain_sink::input_port::source, color_for_sink);
    if (ffmpeg) {
      lewitt::resources::texture_input<lewitt::resources::color_sampled> color_for_ffmpeg{};
      lewitt::resources::wire(forward->outputs().color, color_for_ffmpeg.source);
      graph.record_edge(*forward, *ffmpeg);
      ffmpeg->set_input(nodes::ffmpeg_encode_sink::input_port::source, color_for_ffmpeg);
    }
  }

  void resize(lewitt::gpu_context &ctx) {
    if (!ctx.extent.width || !ctx.extent.height) {
      return;
    }
    if (sink) {
      sink->init(ctx.device, lewitt::present_target::from_context(
                                 ctx.swapchain, ctx.swapchain_format, ctx.extent));
    }
    if (ffmpeg) {
      ffmpeg->resize(ctx.device, ctx.extent);
    }
    graph.set_present_target(
        lewitt::present_target::from_context(ctx.swapchain, ctx.swapchain_format, ctx.extent));
    graph.resize(ctx.device, ctx.extent);
    wire_graph();
    graph.mark_topology_dirty();
  }

  void render(lewitt::gpu_context &ctx,
              const std::function<void(wgpu::RenderPassEncoder &)> &overlay = nullptr) {
    graph.render(ctx, overlay);
  }
};

inline gbuffer_visualizer_pipeline
make_gbuffer_visualizer_pipeline(lewitt::gpu_context &ctx,
                                 const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes,
                                 const std::vector<std::weak_ptr<lewitt::debug_line_buffer>>
                                     &debug_lines = {},
                                 nodes::sink_display_mode mode = nodes::sink_display_mode::normal) {
  return gbuffer_visualizer_pipeline::create(ctx, meshes, debug_lines, mode);
}

inline ssao_deferred_pipeline
make_ssao_deferred_pipeline(lewitt::gpu_context &ctx,
                            const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes,
                            const std::vector<std::weak_ptr<lewitt::debug_line_buffer>>
                                &debug_lines = {},
                            render_pipeline_mode mode = render_pipeline_mode::deferred_lit,
                            const ffmpeg_record_config &record_config = {}) {
  return ssao_deferred_pipeline::create(ctx, meshes, debug_lines, mode, record_config);
}

inline forward_pipeline
make_forward_pipeline(lewitt::gpu_context &ctx,
                      const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes,
                      const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines =
                          {},
                      const ffmpeg_record_config &record_config = {}) {
  return forward_pipeline::create(ctx, meshes, debug_lines, record_config);
}

} // namespace lombardi
