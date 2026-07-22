#pragma once

#include <memory>
#include <stdexcept>
#include <vector>

#include "lewitt/debug_line_buffer.hpp"
#include "lewitt/gpu_session.hpp"
#include "lewitt/mesh_buffer.hpp"
#include "lombardi/gbuffer_graph_helpers.hpp"
#include "lombardi/ffmpeg_record_config.hpp"

namespace lombardi {

enum class render_graph_type { forward, ssao_deferred };

struct render_graph_config {
  render_graph_type type = render_graph_type::ssao_deferred;
  render_pipeline_mode ssao_mode = render_pipeline_mode::deferred_lit;
  ffmpeg_record_config record{};
};

class pipeline_graph {
public:
  virtual ~pipeline_graph() = default;

  virtual void compute(lewitt::gpu_context &ctx) = 0;
  virtual void resize(lewitt::gpu_context &ctx) = 0;
  virtual void set_meshes(const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes) = 0;
  virtual void set_debug_lines(
      const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines) = 0;
};

namespace detail {

class ssao_pipeline_graph final : public pipeline_graph {
public:
  static std::unique_ptr<pipeline_graph>
  create(lewitt::gpu_context &ctx,
         const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes,
         const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines,
         render_pipeline_mode mode, const ffmpeg_record_config &record_config) {
    auto graph = std::make_unique<ssao_pipeline_graph>();
    graph->_pipeline =
        ssao_deferred_pipeline::create(ctx, meshes, debug_lines, mode, record_config);
    graph->_pipeline.rebind_pool();
    return graph;
  }

  void compute(lewitt::gpu_context &ctx) override { _pipeline.render(ctx); }

  void resize(lewitt::gpu_context &ctx) override { _pipeline.resize(ctx); }

  void set_meshes(const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes) override {
    _pipeline.set_meshes(meshes);
  }

  void set_debug_lines(
      const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines) override {
    _pipeline.set_debug_lines(debug_lines);
  }

private:
  ssao_deferred_pipeline _pipeline{};
};

class forward_pipeline_graph final : public pipeline_graph {
public:
  static std::unique_ptr<pipeline_graph>
  create(lewitt::gpu_context &ctx,
         const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes,
         const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines,
         const ffmpeg_record_config &record_config) {
    auto graph = std::make_unique<forward_pipeline_graph>();
    graph->_pipeline = forward_pipeline::create(ctx, meshes, debug_lines, record_config);
    graph->_pipeline.rebind_pool();
    return graph;
  }

  void compute(lewitt::gpu_context &ctx) override { _pipeline.render(ctx); }

  void resize(lewitt::gpu_context &ctx) override { _pipeline.resize(ctx); }

  void set_meshes(const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes) override {
    _pipeline.set_meshes(meshes);
  }

  void set_debug_lines(
      const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines) override {
    _pipeline.set_debug_lines(debug_lines);
  }

private:
  forward_pipeline _pipeline{};
};

} // namespace detail

inline std::unique_ptr<pipeline_graph>
make_pipeline_graph(lewitt::gpu_context &ctx,
                    const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes,
                    const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines,
                    const render_graph_config &config = {}) {
  switch (config.type) {
  case render_graph_type::forward:
    return detail::forward_pipeline_graph::create(ctx, meshes, debug_lines, config.record);
  case render_graph_type::ssao_deferred:
    return detail::ssao_pipeline_graph::create(ctx, meshes, debug_lines, config.ssao_mode,
                                             config.record);
  }
  throw std::invalid_argument("unsupported render_graph_type");
}

} // namespace lombardi

namespace lewitt {
using lombardi::ffmpeg_record_config;
using lombardi::make_pipeline_graph;
using lombardi::pipeline_graph;
using lombardi::render_graph_config;
using lombardi::render_graph_type;
} // namespace lewitt
