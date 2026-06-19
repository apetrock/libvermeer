#pragma once

#include <memory>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "lewitt/bindings.hpp"
#include "lewitt/debug_line_buffer.hpp"
#include "lewitt/frame_context.hpp"
#include "lewitt/mesh_buffer.hpp"
#include "lewitt/nodes.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/resource_handles.hpp"
#include "lewitt/shaders.hpp"

namespace lewitt {
namespace nodes {

class g_buffer_node : public render_node {
public:
  using ptr = std::shared_ptr<g_buffer_node>;

  enum class input { meshes, debug_lines };
  enum class output { position, normal, albedo_spec, depth };

  static ptr create() { return std::make_shared<g_buffer_node>(); }

  void init(wgpu::Device device, render_targets::target_pool &pool,
            const resources::g_buffer_outputs &outputs,
            bindings::uniform::ptr camera_uniform) {
    _pool = &pool;
    _outputs = outputs;
    _camera_uniform = std::move(camera_uniform);

    _mesh_shader = shaders::render_shader::create_from_path(RESOURCE_DIR "/gaudi_gbuffer.wgsl",
                                                           device);
    _mesh_normal_shader =
        shaders::render_shader::create_from_path(RESOURCE_DIR "/gaudi_gbuffer.wgsl", device);
    _line_shader = shaders::render_shader::create_from_path(
        RESOURCE_DIR "/debug_line_gbuffer.wgsl", device);
    _line_normal_shader = shaders::render_shader::create_from_path(
        RESOURCE_DIR "/debug_line_gbuffer.wgsl", device);
    _mesh_albedo_shader =
        shaders::render_shader::create_from_path(RESOURCE_DIR "/gaudi_albedo.wgsl", device);
    _line_albedo_shader = shaders::render_shader::create_from_path(
        RESOURCE_DIR "/debug_line_albedo.wgsl", device);

    _mesh_bindings = make_camera_bindings();
    _mesh_normal_bindings = make_camera_bindings();
    _line_bindings = make_camera_bindings();
    _line_normal_bindings = make_camera_bindings();
    _mesh_albedo_bindings = make_camera_bindings();
    _line_albedo_bindings = make_camera_bindings();

    reset_pipelines();
  }

  void resize(wgpu::Device device) {
    _mesh_shader = shaders::render_shader::create_from_path(RESOURCE_DIR "/gaudi_gbuffer.wgsl",
                                                           device);
    _mesh_normal_shader =
        shaders::render_shader::create_from_path(RESOURCE_DIR "/gaudi_gbuffer.wgsl", device);
    _line_shader = shaders::render_shader::create_from_path(
        RESOURCE_DIR "/debug_line_gbuffer.wgsl", device);
    _line_normal_shader = shaders::render_shader::create_from_path(
        RESOURCE_DIR "/debug_line_gbuffer.wgsl", device);
    _mesh_albedo_shader =
        shaders::render_shader::create_from_path(RESOURCE_DIR "/gaudi_albedo.wgsl", device);
    _line_albedo_shader = shaders::render_shader::create_from_path(
        RESOURCE_DIR "/debug_line_albedo.wgsl", device);
    reset_pipelines();
  }

  void set_input(input, const std::vector<std::weak_ptr<mesh_buffer>> &meshes) {
    _meshes = meshes;
  }

  void set_input(input, const std::vector<std::weak_ptr<debug_line_buffer>> &debug_lines) {
    _debug_lines = debug_lines;
  }

  const resources::g_buffer_outputs &outputs() const { return _outputs; }

  void set_pool(render_targets::target_pool &pool) { _pool = &pool; }

  std::vector<resource_use> resource_uses() const override {
    std::vector<resource_use> uses;
    if (_outputs.position.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _outputs.position.id});
    }
    if (_outputs.normal.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _outputs.normal.id});
    }
    if (_outputs.albedo_spec.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _outputs.albedo_spec.id});
    }
    if (_outputs.depth.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _outputs.depth.id});
    }
    return uses;
  }

  void render(frame_context &ctx) override {
    wgpu::Device device = ctx.device;
    if (!_pool || _outputs.position.id == render_targets::invalid_target_id) {
      return;
    }
    if (_meshes.empty() && _debug_lines.empty()) {
      return;
    }

    ensure_mesh_pipeline(device);
    ensure_mesh_normal_pipeline(device);
    ensure_line_pipeline(device);
    ensure_line_normal_pipeline(device);
    ensure_mesh_albedo_pipeline(device);
    ensure_line_albedo_pipeline(device);
    if (!_mesh_pipeline_ready || !_mesh_normal_pipeline_ready) {
      return;
    }

    render_targets::frame_attachments position_attachments{};
    position_attachments.color_target = _outputs.position.id;

    passes::render(nullptr, device, *_pool, position_attachments,
                   [&](wgpu::RenderPassEncoder &enc, wgpu::Device &dev) {
                     draw_meshes(enc, dev);
                     draw_debug_lines(enc, dev);
                   },
                   false);

    render_targets::frame_attachments normal_attachments{};
    normal_attachments.color_target = _outputs.normal.id;

    passes::render(nullptr, device, *_pool, normal_attachments,
                   [&](wgpu::RenderPassEncoder &enc, wgpu::Device &dev) {
                     draw_mesh_normals(enc, dev);
                     draw_debug_line_normals(enc, dev);
                   },
                   false);

    if (_outputs.albedo_spec.id == render_targets::invalid_target_id) {
      return;
    }

    render_targets::frame_attachments albedo_attachments{};
    albedo_attachments.color_target = _outputs.albedo_spec.id;

    passes::render(nullptr, device, *_pool, albedo_attachments,
                   [&](wgpu::RenderPassEncoder &enc, wgpu::Device &dev) {
                     draw_mesh_albedo(enc, dev);
                     draw_debug_line_albedo(enc, dev);
                   },
                   false);
  }

private:
  bindings::group::ptr make_camera_bindings() {
    auto group = bindings::group::create();
    if (_camera_uniform) {
      _camera_uniform->set_visibility(wgpu::ShaderStage::Vertex);
      group->append(_camera_uniform);
    }
    return group;
  }

  void reset_pipelines() {
    _mesh_pipeline_ready = false;
    _mesh_normal_pipeline_ready = false;
    _line_pipeline_ready = false;
    _line_normal_pipeline_ready = false;
    _mesh_albedo_pipeline_ready = false;
    _line_albedo_pipeline_ready = false;
  }

  void draw_meshes(wgpu::RenderPassEncoder &enc, wgpu::Device &device) {
    if (!_mesh_pipeline_ready || _meshes.empty()) {
      return;
    }
    enc.setPipeline(_mesh_shader->render_pipe_line());
    enc.setBindGroup(0, _mesh_bindings->get_group(), 0, nullptr);

    for (const auto &mesh_ref : _meshes) {
      auto mesh = mesh_ref.lock();
      if (!mesh || !mesh->valid()) {
        continue;
      }
      enc.setVertexBuffer(0, mesh->vertex_buffer()->get_buffer(), 0,
                          mesh->vertex_buffer()->get_buffer().getSize());
      enc.setIndexBuffer(mesh->index_buffer()->get_buffer(), wgpu::IndexFormat::Uint32, 0,
                         mesh->index_buffer()->size());
      enc.drawIndexed(mesh->index_count(), 1, 0, 0, 0);
    }
  }

  void draw_mesh_normals(wgpu::RenderPassEncoder &enc, wgpu::Device &device) {
    if (!_mesh_normal_pipeline_ready || _meshes.empty()) {
      return;
    }
    enc.setPipeline(_mesh_normal_shader->render_pipe_line());
    enc.setBindGroup(0, _mesh_normal_bindings->get_group(), 0, nullptr);

    for (const auto &mesh_ref : _meshes) {
      auto mesh = mesh_ref.lock();
      if (!mesh || !mesh->valid()) {
        continue;
      }
      enc.setVertexBuffer(0, mesh->vertex_buffer()->get_buffer(), 0,
                          mesh->vertex_buffer()->get_buffer().getSize());
      enc.setIndexBuffer(mesh->index_buffer()->get_buffer(), wgpu::IndexFormat::Uint32, 0,
                         mesh->index_buffer()->size());
      enc.drawIndexed(mesh->index_count(), 1, 0, 0, 0);
    }
  }

  void draw_debug_lines(wgpu::RenderPassEncoder &enc, wgpu::Device &device) {
    if (!_line_pipeline_ready) {
      return;
    }
    enc.setPipeline(_line_shader->render_pipe_line());
    enc.setBindGroup(0, _line_bindings->get_group(), 0, nullptr);

    for (const auto &line_ref : _debug_lines) {
      auto lines = line_ref.lock();
      if (!lines || !lines->valid()) {
        continue;
      }
      lines->upload(device);
      enc.setVertexBuffer(0, lines->position_buffer()->get_buffer(), 0,
                          lines->position_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(1, lines->normal_buffer()->get_buffer(), 0,
                          lines->normal_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(2, lines->flag_buffer()->get_buffer(), 0,
                          lines->flag_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(3, lines->radius_buffer()->get_buffer(), 0,
                          lines->radius_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(4, lines->p0_buffer()->get_buffer(), 0,
                          lines->p0_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(5, lines->p1_buffer()->get_buffer(), 0,
                          lines->p1_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(6, lines->color_buffer()->get_buffer(), 0,
                          lines->color_buffer()->get_buffer().getSize());
      enc.setIndexBuffer(lines->index_buffer()->get_buffer(), wgpu::IndexFormat::Uint32, 0,
                         lines->index_buffer()->size());
      enc.drawIndexed(lines->index_count(), lines->instance_count(), 0, 0, 0);
    }
  }

  void draw_debug_line_normals(wgpu::RenderPassEncoder &enc, wgpu::Device &device) {
    if (!_line_normal_pipeline_ready) {
      return;
    }
    enc.setPipeline(_line_normal_shader->render_pipe_line());
    enc.setBindGroup(0, _line_normal_bindings->get_group(), 0, nullptr);

    for (const auto &line_ref : _debug_lines) {
      auto lines = line_ref.lock();
      if (!lines || !lines->valid()) {
        continue;
      }
      lines->upload(device);
      enc.setVertexBuffer(0, lines->position_buffer()->get_buffer(), 0,
                          lines->position_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(1, lines->normal_buffer()->get_buffer(), 0,
                          lines->normal_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(2, lines->flag_buffer()->get_buffer(), 0,
                          lines->flag_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(3, lines->radius_buffer()->get_buffer(), 0,
                          lines->radius_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(4, lines->p0_buffer()->get_buffer(), 0,
                          lines->p0_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(5, lines->p1_buffer()->get_buffer(), 0,
                          lines->p1_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(6, lines->color_buffer()->get_buffer(), 0,
                          lines->color_buffer()->get_buffer().getSize());
      enc.setIndexBuffer(lines->index_buffer()->get_buffer(), wgpu::IndexFormat::Uint32, 0,
                         lines->index_buffer()->size());
      enc.drawIndexed(lines->index_count(), lines->instance_count(), 0, 0, 0);
    }
  }

  void draw_mesh_albedo(wgpu::RenderPassEncoder &enc, wgpu::Device &device) {
    if (!_mesh_albedo_pipeline_ready || _meshes.empty()) {
      return;
    }
    enc.setPipeline(_mesh_albedo_shader->render_pipe_line());
    enc.setBindGroup(0, _mesh_albedo_bindings->get_group(), 0, nullptr);

    for (const auto &mesh_ref : _meshes) {
      auto mesh = mesh_ref.lock();
      if (!mesh || !mesh->valid()) {
        continue;
      }
      enc.setVertexBuffer(0, mesh->vertex_buffer()->get_buffer(), 0,
                          mesh->vertex_buffer()->get_buffer().getSize());
      enc.setIndexBuffer(mesh->index_buffer()->get_buffer(), wgpu::IndexFormat::Uint32, 0,
                         mesh->index_buffer()->size());
      enc.drawIndexed(mesh->index_count(), 1, 0, 0, 0);
    }
  }

  void draw_debug_line_albedo(wgpu::RenderPassEncoder &enc, wgpu::Device &device) {
    if (!_line_albedo_pipeline_ready) {
      return;
    }
    enc.setPipeline(_line_albedo_shader->render_pipe_line());
    enc.setBindGroup(0, _line_albedo_bindings->get_group(), 0, nullptr);

    for (const auto &line_ref : _debug_lines) {
      auto lines = line_ref.lock();
      if (!lines || !lines->valid()) {
        continue;
      }
      lines->upload(device);
      enc.setVertexBuffer(0, lines->position_buffer()->get_buffer(), 0,
                          lines->position_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(1, lines->normal_buffer()->get_buffer(), 0,
                          lines->normal_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(2, lines->flag_buffer()->get_buffer(), 0,
                          lines->flag_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(3, lines->radius_buffer()->get_buffer(), 0,
                          lines->radius_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(4, lines->p0_buffer()->get_buffer(), 0,
                          lines->p0_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(5, lines->p1_buffer()->get_buffer(), 0,
                          lines->p1_buffer()->get_buffer().getSize());
      enc.setVertexBuffer(6, lines->color_buffer()->get_buffer(), 0,
                          lines->color_buffer()->get_buffer().getSize());
      enc.setIndexBuffer(lines->index_buffer()->get_buffer(), wgpu::IndexFormat::Uint32, 0,
                         lines->index_buffer()->size());
      enc.drawIndexed(lines->index_count(), lines->instance_count(), 0, 0, 0);
    }
  }

  void ensure_mesh_pipeline(wgpu::Device device) {
    if (_mesh_pipeline_ready || !_mesh_shader || !_mesh_bindings || _meshes.empty() || !_pool) {
      return;
    }

    _mesh_shader->add_layout(mesh_buffer::layout());
    _mesh_bindings->init_layout(device);
    _mesh_bindings->init(device);

    if (!_mesh_shader->init(device, _mesh_bindings->get_layout(),
                            resources::position_attachment::format(),
                            wgpu::TextureFormat::Undefined, "vs_gbuffer",
                            "fs_position")) {
      return;
    }
    _mesh_pipeline_ready = _mesh_shader->render_pipe_line() != nullptr;
  }

  void ensure_mesh_normal_pipeline(wgpu::Device device) {
    if (_mesh_normal_pipeline_ready || !_mesh_normal_shader || !_mesh_normal_bindings ||
        _meshes.empty() || !_pool) {
      return;
    }

    _mesh_normal_shader->add_layout(mesh_buffer::layout());
    _mesh_normal_bindings->init_layout(device);
    _mesh_normal_bindings->init(device);

    if (!_mesh_normal_shader->init(device, _mesh_normal_bindings->get_layout(),
                                   resources::normal_attachment::format(),
                                   wgpu::TextureFormat::Undefined, "vs_gbuffer",
                                   "fs_normal")) {
      return;
    }
    _mesh_normal_pipeline_ready = _mesh_normal_shader->render_pipe_line() != nullptr;
  }

  void ensure_line_pipeline(wgpu::Device device) {
    if (_line_pipeline_ready || !_line_shader || !_line_bindings || !_pool) {
      return;
    }

    debug_line_buffer::ptr layout_source;
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

    for (const auto &layout : layout_source->vertex_layouts()) {
      _line_shader->add_layout(layout);
    }
    _line_bindings->init_layout(device);
    _line_bindings->init(device);

    if (!_line_shader->init(device, _line_bindings->get_layout(),
                            resources::position_attachment::format(),
                            wgpu::TextureFormat::Undefined, "vs_gbuffer",
                            "fs_position")) {
      return;
    }
    _line_pipeline_ready = _line_shader->render_pipe_line() != nullptr;
  }

  void ensure_line_normal_pipeline(wgpu::Device device) {
    if (_line_normal_pipeline_ready || !_line_normal_shader || !_line_normal_bindings || !_pool) {
      return;
    }

    debug_line_buffer::ptr layout_source;
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

    for (const auto &layout : layout_source->vertex_layouts()) {
      _line_normal_shader->add_layout(layout);
    }
    _line_normal_bindings->init_layout(device);
    _line_normal_bindings->init(device);

    if (!_line_normal_shader->init(device, _line_normal_bindings->get_layout(),
                                   resources::normal_attachment::format(),
                                   wgpu::TextureFormat::Undefined, "vs_gbuffer",
                                   "fs_normal")) {
      return;
    }
    _line_normal_pipeline_ready = _line_normal_shader->render_pipe_line() != nullptr;
  }

  void ensure_mesh_albedo_pipeline(wgpu::Device device) {
    if (_mesh_albedo_pipeline_ready || !_mesh_albedo_shader || !_mesh_albedo_bindings ||
        _meshes.empty() || !_pool) {
      return;
    }

    _mesh_albedo_shader->add_layout(mesh_buffer::layout());
    _mesh_albedo_bindings->init_layout(device);
    _mesh_albedo_bindings->init(device);

    if (!_mesh_albedo_shader->init(device, _mesh_albedo_bindings->get_layout(),
                                   resources::albedo_spec_attachment::format(),
                                   wgpu::TextureFormat::Undefined, "vs_albedo",
                                   "fs_albedo")) {
      return;
    }
    _mesh_albedo_pipeline_ready = _mesh_albedo_shader->render_pipe_line() != nullptr;
  }

  void ensure_line_albedo_pipeline(wgpu::Device device) {
    if (_line_albedo_pipeline_ready || !_line_albedo_shader || !_line_albedo_bindings || !_pool) {
      return;
    }

    debug_line_buffer::ptr layout_source;
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

    for (const auto &layout : layout_source->vertex_layouts()) {
      _line_albedo_shader->add_layout(layout);
    }
    _line_albedo_bindings->init_layout(device);
    _line_albedo_bindings->init(device);

    if (!_line_albedo_shader->init(device, _line_albedo_bindings->get_layout(),
                                   resources::albedo_spec_attachment::format(),
                                   wgpu::TextureFormat::Undefined, "vs_albedo",
                                   "fs_albedo")) {
      return;
    }
    _line_albedo_pipeline_ready = _line_albedo_shader->render_pipe_line() != nullptr;
  }

  render_targets::target_pool *_pool = nullptr;
  resources::g_buffer_outputs _outputs{};
  std::vector<std::weak_ptr<mesh_buffer>> _meshes;
  std::vector<std::weak_ptr<debug_line_buffer>> _debug_lines;
  bindings::uniform::ptr _camera_uniform;
  shaders::render_shader::ptr _mesh_shader;
  shaders::render_shader::ptr _mesh_normal_shader;
  shaders::render_shader::ptr _line_shader;
  shaders::render_shader::ptr _line_normal_shader;
  shaders::render_shader::ptr _mesh_albedo_shader;
  shaders::render_shader::ptr _line_albedo_shader;
  bindings::group::ptr _mesh_bindings;
  bindings::group::ptr _mesh_normal_bindings;
  bindings::group::ptr _line_bindings;
  bindings::group::ptr _line_normal_bindings;
  bindings::group::ptr _mesh_albedo_bindings;
  bindings::group::ptr _line_albedo_bindings;
  bool _mesh_pipeline_ready = false;
  bool _mesh_normal_pipeline_ready = false;
  bool _line_pipeline_ready = false;
  bool _line_normal_pipeline_ready = false;
  bool _mesh_albedo_pipeline_ready = false;
  bool _line_albedo_pipeline_ready = false;
};

} // namespace nodes
} // namespace lewitt
