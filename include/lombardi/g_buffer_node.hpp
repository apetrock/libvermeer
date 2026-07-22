#pragma once

#include <memory>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "lewitt/bindings.hpp"
#include "lewitt/debug_line_buffer.hpp"
#include "lewitt/frame_context.hpp"
#include "lewitt/mesh_buffer.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/performance.hpp"
#include "lewitt/resource_handles.hpp"
#include "lombardi/nodes.hpp"
#include "mondrian/draw_bundles.hpp"
#include "mondrian/draw_schema.hpp"
#include "mondrian/render_contract.hpp"

namespace lombardi {
namespace nodes {

class g_buffer_node : public render_node {
public:
  using ptr = std::shared_ptr<g_buffer_node>;

  enum class input { camera, meshes, debug_lines };
  enum class output { position, normal, albedo_spec, depth };

  static constexpr int k_pass_count_with_geometry = 2;

  static ptr create() { return std::make_shared<g_buffer_node>(); }

  void init(wgpu::Device device, lewitt::render_targets::target_pool &pool,
            const lewitt::resources::g_buffer_outputs &outputs) {
    _pool = &pool;
    _outputs = outputs;
    _pipeline_state.clear();
    _mesh_bindings = make_camera_bindings();
    _line_bindings = make_camera_bindings();
  }

  void resize(wgpu::Device device) {
    (void)device;
    _pipeline_state.clear();
  }

  void set_input(input, lewitt::bindings::uniform::ptr camera_uniform) {
    _camera_uniform = std::move(camera_uniform);
    _mesh_bindings = make_camera_bindings();
    _line_bindings = make_camera_bindings();
    _bindings_ready = false;
    _pipeline_state.clear();
  }

  void set_input(input, const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes) {
    _meshes = meshes;
  }

  void set_input(input, const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines) {
    _debug_lines = debug_lines;
  }

  const lewitt::resources::g_buffer_outputs &outputs() const { return _outputs; }

  void set_pool(lewitt::render_targets::target_pool &pool) { _pool = &pool; }

  int pass_count() const {
    return mondrian::render_contract::gbuffer_pass_count(!_meshes.empty(), !_debug_lines.empty());
  }

  std::vector<resource_use> resource_uses() const override {
    std::vector<resource_use> uses;
    if (_outputs.position.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _outputs.position.id});
    }
    if (_outputs.normal.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _outputs.normal.id});
    }
    if (_outputs.albedo_spec.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _outputs.albedo_spec.id});
    }
    if (_outputs.depth.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _outputs.depth.id});
    }
    return uses;
  }

  void render(lewitt::frame_context &ctx) override {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::g_buffer_node::render");
    wgpu::Device device = ctx.device;
    if (!_pool || _outputs.position.id == lewitt::render_targets::invalid_target_id) {
      return;
    }
    if (_meshes.empty() && _debug_lines.empty()) {
      return;
    }

    ensure_bindings(device);
    render_mrt_pass(device);
    render_albedo_pass(device);
  }

private:
  void ensure_bindings(wgpu::Device device) {
    if (_bindings_ready || !_camera_uniform) {
      return;
    }
    if (_mesh_bindings) {
      _mesh_bindings->init_layout(device);
      _mesh_bindings->init(device);
    }
    if (_line_bindings) {
      _line_bindings->init_layout(device);
      _line_bindings->init(device);
    }
    _bindings_ready = true;
  }

  lewitt::bindings::group::ptr make_camera_bindings() {
    auto group = lewitt::bindings::group::create();
    if (_camera_uniform) {
      _camera_uniform->set_visibility(wgpu::ShaderStage::Vertex);
      group->append(_camera_uniform);
    }
    return group;
  }

  void render_mrt_pass(wgpu::Device device) {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::g_buffer_node::render_mrt_pass");
    lewitt::render_targets::frame_attachments attachments{};
    attachments.color_targets = {_outputs.position.id, _outputs.normal.id};
    attachments.depth_target = _outputs.depth.id;

    lewitt::passes::render(nullptr, device, *_pool, attachments,
                           [&](wgpu::RenderPassEncoder &enc, wgpu::Device &dev) {
                             record_mesh_archetype(enc, dev, mondrian::mesh_gbuffer_mrt_schema());
                             record_line_archetype(enc, dev, mondrian::debug_line_gbuffer_mrt_schema());
                           },
                           false);
  }

  void render_albedo_pass(wgpu::Device device) {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::g_buffer_node::render_albedo_pass");
    if (_outputs.albedo_spec.id == lewitt::render_targets::invalid_target_id) {
      return;
    }

    lewitt::render_targets::frame_attachments attachments{};
    attachments.color_target = _outputs.albedo_spec.id;
    attachments.depth_target = _outputs.depth.id;
    attachments.depth_state = lewitt::render_targets::attachment_state::depth_load_readonly();

    lewitt::passes::render(nullptr, device, *_pool, attachments,
                           [&](wgpu::RenderPassEncoder &enc, wgpu::Device &dev) {
                             record_mesh_archetype(enc, dev, mondrian::mesh_albedo_schema());
                             record_line_archetype(enc, dev, mondrian::debug_line_albedo_schema());
                           },
                           false);
  }

  void record_mesh_archetype(wgpu::RenderPassEncoder &enc, wgpu::Device &device,
                             const mondrian::draw_schema &schema) {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::g_buffer_node::record_mesh_archetype");
    if (_meshes.empty() || !_mesh_bindings || !_mesh_bindings->get_group()) {
      return;
    }

    auto pipeline = _pipeline_state.get_or_create(device, schema, _mesh_bindings);
    if (!pipeline) {
      return;
    }

    for (const auto &mesh_ref : _meshes) {
      auto mesh = mesh_ref.lock();
      if (!mesh || !mesh->valid()) {
        continue;
      }
      const auto vertices = mondrian::make_mesh_vertex_bundle(*mesh);
      mondrian::binding_bundle bindings{};
      bindings.groups = {_mesh_bindings};
      mondrian::record(enc, pipeline, vertices, bindings);
    }
  }

  void record_line_archetype(wgpu::RenderPassEncoder &enc, wgpu::Device &device,
                             mondrian::draw_schema schema) {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::g_buffer_node::record_line_archetype");
    if (_debug_lines.empty() || !_line_bindings || !_line_bindings->get_group()) {
      return;
    }

    lewitt::debug_line_buffer::ptr layout_source;
    for (const auto &line_ref : _debug_lines) {
      if (auto lines = line_ref.lock()) {
        lines->ensure_static_geometry(device);
        layout_source = lines;
        break;
      }
    }
    if (!layout_source) {
      return;
    }

    schema.vertex_layouts = layout_source->vertex_layouts();
    auto pipeline = _pipeline_state.get_or_create(device, schema, _line_bindings);
    if (!pipeline) {
      return;
    }

    for (const auto &line_ref : _debug_lines) {
      auto lines = line_ref.lock();
      if (!lines || !lines->valid()) {
        continue;
      }
      lines->upload(device);
      const auto vertices = mondrian::make_debug_line_vertex_bundle(*lines);
      mondrian::binding_bundle bindings{};
      bindings.groups = {_line_bindings};
      mondrian::record(enc, pipeline, vertices, bindings);
    }
  }

  lewitt::render_targets::target_pool *_pool = nullptr;
  lewitt::resources::g_buffer_outputs _outputs{};
  std::vector<std::weak_ptr<lewitt::mesh_buffer>> _meshes;
  std::vector<std::weak_ptr<lewitt::debug_line_buffer>> _debug_lines;
  lewitt::bindings::uniform::ptr _camera_uniform;
  lewitt::bindings::group::ptr _mesh_bindings;
  lewitt::bindings::group::ptr _line_bindings;
  bool _bindings_ready = false;
  mondrian::pipeline_state _pipeline_state;
};

} // namespace nodes
} // namespace lombardi
