#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <webgpu/webgpu.hpp>

#include "common.h"
#include "lewitt/bindings.hpp"
#include "lewitt/frame_context.hpp"
#include "lewitt/nodes.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/resource_handles.hpp"
#include "lewitt/shaders.hpp"

namespace lewitt {
namespace nodes {

class ssao_node : public render_node {
public:
  using ptr = std::shared_ptr<ssao_node>;

  enum class input { position, normal, camera };
  enum class output { ao };

  static ptr create() { return std::make_shared<ssao_node>(); }

  void init(wgpu::Device device, render_targets::target_pool &pool,
            resources::handle<resources::ssao_attachment> ao_target,
            bindings::uniform::ptr camera_uniform) {
    _pool = &pool;
    _ao_target = ao_target;
    _camera_uniform = std::move(camera_uniform);

    _shader = shaders::render_shader::create_from_path(RESOURCE_DIR "/ssao.wgsl", device);
    _bindings = bindings::group::create();

    _position_input = bindings::borrowed_texture_view::create();
    _position_input->set_id(0);
    _position_input->set_frag_unfilterable_float_2d();

    _normal_input = bindings::borrowed_texture_view::create();
    _normal_input->set_id(1);
    _normal_input->set_frag_unfilterable_float_2d();

    _sampler = bindings::default_nearest_sampler(wgpu::ShaderStage::Fragment, device);
    _sampler->set_id(2);

    _params = bindings::uniform::create<mat4, vec4>(
        {"projectionMatrix", "params"}, device);
    _params->set_visibility(wgpu::ShaderStage::Fragment);
    _params->set_id(3);
    _params->set_member("params", vec4(0.75f, 0.05f, 0.0f, 0.0f));

    _bindings->append(_position_input);
    _bindings->append(_normal_input);
    _bindings->append(_sampler);
    _bindings->append(_params);

    _pipeline_ready = false;
  }

  void resize(wgpu::Device device) {
    _shader = shaders::render_shader::create_from_path(RESOURCE_DIR "/ssao.wgsl", device);
    _pipeline_ready = false;
  }

  void set_input(input, const resources::texture_input<resources::position_sampled> &position) {
    _position = position;
  }

  void set_input(input, const resources::texture_input<resources::normal_sampled> &normal) {
    _normal = normal;
  }

  resources::handle<resources::ssao_attachment> output() const { return _ao_target; }

  void set_pool(render_targets::target_pool &pool) { _pool = &pool; }

  std::vector<resource_use> resource_uses() const override {
    std::vector<resource_use> uses;
    if (_position.source.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _position.source.id});
    }
    if (_normal.source.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _normal.source.id});
    }
    if (_ao_target.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _ao_target.id});
    }
    return uses;
  }

  void render(frame_context &ctx) override {
    if (!_pool || !_shader || _ao_target.id == render_targets::invalid_target_id) {
      return;
    }
    if (_position.source.id == render_targets::invalid_target_id ||
        _normal.source.id == render_targets::invalid_target_id) {
      return;
    }

    wgpu::Device device = ctx.device;
    ensure_pipeline(device);
    if (!_pipeline_ready) {
      return;
    }

    if (_camera_uniform) {
      _params->set_member("projectionMatrix",
                          _camera_uniform->get_member<mat4>("projectionMatrix"));
      _params->update(ctx.queue);
    }

    _position_input->set_view(_pool->get(_position.source.id).view());
    _normal_input->set_view(_pool->get(_normal.source.id).view());
    _bindings->init(device);

    render_targets::frame_attachments attachments{};
    attachments.color_targets = {_ao_target.id};

    passes::render(nullptr, device, *_pool, attachments,
                   [&](wgpu::RenderPassEncoder &enc, wgpu::Device &) {
                     enc.setPipeline(_shader->render_pipe_line());
                     enc.setBindGroup(0, _bindings->get_group(), 0, nullptr);
                     enc.draw(3, 1, 0, 0);
                   },
                   false);
  }

private:
  void ensure_pipeline(wgpu::Device device) {
    if (_pipeline_ready || !_shader || !_bindings) {
      return;
    }

    _bindings->init_layout(device);

    if (!_shader->init(device, _bindings->get_layout(),
                       resources::ssao_attachment::format(),
                       wgpu::TextureFormat::Undefined, "vs_main", "fs_main")) {
      _pipeline_ready = false;
      return;
    }
    _pipeline_ready = _shader->render_pipe_line() != nullptr;
  }

  render_targets::target_pool *_pool = nullptr;
  resources::handle<resources::ssao_attachment> _ao_target{};
  resources::texture_input<resources::position_sampled> _position{};
  resources::texture_input<resources::normal_sampled> _normal{};
  bindings::uniform::ptr _camera_uniform;
  bindings::uniform::ptr _params;
  bindings::group::ptr _bindings;
  bindings::borrowed_texture_view::ptr _position_input;
  bindings::borrowed_texture_view::ptr _normal_input;
  bindings::sampler::ptr _sampler;
  shaders::render_shader::ptr _shader;
  bool _pipeline_ready = false;
};

} // namespace nodes
} // namespace lewitt
