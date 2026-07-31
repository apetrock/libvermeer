#pragma once

#include <memory>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "lewitt/bindings.hpp"
#include "lewitt/debug_line_buffer.hpp"
#include "lewitt/debug_sphere_buffer.hpp"
#include "lewitt/debug_torus_buffer.hpp"
#include "lewitt/frame_context.hpp"
#include "lewitt/mesh_buffer.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/performance.hpp"
#include "lewitt/resource_handles.hpp"
#include "lombardi/nodes.hpp"
#include "mondrian/draw_bundles.hpp"
#include "mondrian/draw_schema.hpp"

namespace lombardi {
namespace nodes {

struct forward_outputs {
  lewitt::resources::handle<lewitt::resources::lit_color_attachment> color;
  lewitt::resources::handle<lewitt::resources::depth_attachment> depth;
};

class forward_mesh_node : public render_node {
public:
  using ptr = std::shared_ptr<forward_mesh_node>;

  enum class input { camera, lighting, meshes, debug_lines, debug_spheres, debug_tori };
  enum class output { color, depth };

  static ptr create() { return std::make_shared<forward_mesh_node>(); }

  void init(wgpu::Device device, lewitt::render_targets::target_pool &pool,
            const forward_outputs &outputs) {
    _pool = &pool;
    _outputs = outputs;
    _pipeline_state.clear();
    _mesh_bindings = make_mesh_bindings();
    _line_bindings = make_line_bindings();
  }

  void resize(wgpu::Device device) {
    (void)device;
    _pipeline_state.clear();
  }

  void set_input(input port, lewitt::bindings::uniform::ptr uniform) {
    switch (port) {
    case input::camera:
      _camera_uniform = std::move(uniform);
      break;
    case input::lighting:
      _lighting_uniform = std::move(uniform);
      break;
    default:
      break;
    }
    _mesh_bindings = make_mesh_bindings();
    _line_bindings = make_line_bindings();
    _bindings_ready = false;
    _pipeline_state.clear();
  }

  void set_input(input port, const std::vector<std::weak_ptr<lewitt::mesh_buffer>> &meshes) {
    if (port != input::meshes) {
      return;
    }
    _meshes = meshes;
  }

  void set_input(input port,
                 const std::vector<std::weak_ptr<lewitt::debug_line_buffer>> &debug_lines) {
    if (port != input::debug_lines) {
      return;
    }
    _debug_lines = debug_lines;
  }

  void set_input(input port,
                 const std::vector<std::weak_ptr<lewitt::debug_sphere_buffer>> &debug_spheres) {
    if (port != input::debug_spheres) {
      return;
    }
    _debug_spheres = debug_spheres;
  }

  void set_input(input port,
                 const std::vector<std::weak_ptr<lewitt::debug_torus_buffer>> &debug_tori) {
    if (port != input::debug_tori) {
      return;
    }
    _debug_tori = debug_tori;
  }

  const forward_outputs &outputs() const { return _outputs; }

  void set_pool(lewitt::render_targets::target_pool &pool) { _pool = &pool; }

  std::vector<resource_use> resource_uses() const override {
    std::vector<resource_use> uses;
    if (_outputs.color.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _outputs.color.id});
    }
    if (_outputs.depth.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _outputs.depth.id});
    }
    return uses;
  }

  void render(lewitt::frame_context &ctx) override {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::forward_mesh_node::render");
    wgpu::Device device = ctx.device;
    if (!_pool || _outputs.color.id == lewitt::render_targets::invalid_target_id) {
      return;
    }
    if (_meshes.empty() && _debug_lines.empty() && _debug_spheres.empty() &&
        _debug_tori.empty()) {
      return;
    }

    ensure_bindings(device);

    lewitt::render_targets::frame_attachments attachments{};
    attachments.color_target = _outputs.color.id;
    attachments.depth_target = _outputs.depth.id;

    lewitt::passes::render(nullptr, device, *_pool, attachments,
                           [&](wgpu::RenderPassEncoder &enc, wgpu::Device &dev) {
                             record_mesh_archetype(enc, dev, mondrian::mesh_forward_schema());
                             record_line_archetype(enc, dev, mondrian::debug_line_forward_schema());
                             record_sphere_archetype(enc, dev, mondrian::debug_sphere_forward_schema());
                             record_torus_archetype(enc, dev, mondrian::debug_torus_forward_schema());
                           },
                           false);
  }

private:
  void ensure_bindings(wgpu::Device device) {
    if (_bindings_ready) {
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

  lewitt::bindings::group::ptr make_mesh_bindings() {
    auto group = lewitt::bindings::group::create();
    if (_camera_uniform) {
      _camera_uniform->set_visibility(wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
      group->append(_camera_uniform);
    }
    if (_lighting_uniform) {
      _lighting_uniform->set_visibility(wgpu::ShaderStage::Fragment);
      group->append(_lighting_uniform);
    }
    return group;
  }

  lewitt::bindings::group::ptr make_line_bindings() {
    auto group = lewitt::bindings::group::create();
    if (_camera_uniform) {
      _camera_uniform->set_visibility(wgpu::ShaderStage::Vertex);
      group->append(_camera_uniform);
    }
    return group;
  }

  void record_mesh_archetype(wgpu::RenderPassEncoder &enc, wgpu::Device &device,
                             const mondrian::draw_schema &schema) {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::forward_mesh_node::record_mesh_archetype");
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
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::forward_mesh_node::record_line_archetype");
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

  void record_sphere_archetype(wgpu::RenderPassEncoder &enc, wgpu::Device &device,
                               mondrian::draw_schema schema) {
    if (_debug_spheres.empty() || !_line_bindings || !_line_bindings->get_group()) {
      return;
    }
    lewitt::debug_sphere_buffer::ptr layout_source;
    for (const auto &ref : _debug_spheres) {
      if (auto buf = ref.lock()) {
        buf->ensure_static_geometry(device);
        layout_source = buf;
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
    for (const auto &ref : _debug_spheres) {
      auto buf = ref.lock();
      if (!buf || !buf->valid()) {
        continue;
      }
      buf->upload(device);
      const auto vertices = mondrian::make_debug_sphere_vertex_bundle(*buf);
      mondrian::binding_bundle bindings{};
      bindings.groups = {_line_bindings};
      mondrian::record(enc, pipeline, vertices, bindings);
    }
  }

  void record_torus_archetype(wgpu::RenderPassEncoder &enc, wgpu::Device &device,
                              mondrian::draw_schema schema) {
    if (_debug_tori.empty() || !_line_bindings || !_line_bindings->get_group()) {
      return;
    }
    lewitt::debug_torus_buffer::ptr layout_source;
    for (const auto &ref : _debug_tori) {
      if (auto buf = ref.lock()) {
        buf->ensure_static_geometry(device);
        layout_source = buf;
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
    for (const auto &ref : _debug_tori) {
      auto buf = ref.lock();
      if (!buf || !buf->valid()) {
        continue;
      }
      buf->upload(device);
      const auto vertices = mondrian::make_debug_torus_vertex_bundle(*buf);
      mondrian::binding_bundle bindings{};
      bindings.groups = {_line_bindings};
      mondrian::record(enc, pipeline, vertices, bindings);
    }
  }

  lewitt::render_targets::target_pool *_pool = nullptr;
  forward_outputs _outputs{};
  std::vector<std::weak_ptr<lewitt::mesh_buffer>> _meshes;
  std::vector<std::weak_ptr<lewitt::debug_line_buffer>> _debug_lines;
  std::vector<std::weak_ptr<lewitt::debug_sphere_buffer>> _debug_spheres;
  std::vector<std::weak_ptr<lewitt::debug_torus_buffer>> _debug_tori;
  lewitt::bindings::uniform::ptr _camera_uniform;
  lewitt::bindings::uniform::ptr _lighting_uniform;
  lewitt::bindings::group::ptr _mesh_bindings;
  lewitt::bindings::group::ptr _line_bindings;
  bool _bindings_ready = false;
  mondrian::pipeline_state _pipeline_state;
};

} // namespace nodes
} // namespace lombardi
