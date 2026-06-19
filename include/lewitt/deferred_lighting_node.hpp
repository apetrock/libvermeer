#pragma once

#include <memory>

#include <webgpu/webgpu.hpp>

#include "lewitt/bindings.hpp"
#include "lewitt/frame_context.hpp"
#include "lewitt/nodes.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/resource_handles.hpp"
#include "lewitt/shaders.hpp"

namespace lewitt {
namespace nodes {

class deferred_lighting_node : public render_node {
public:
  using ptr = std::shared_ptr<deferred_lighting_node>;

  enum class input { position, normal, albedo_spec, ssao };
  enum class output { lit_color };

  static ptr create() { return std::make_shared<deferred_lighting_node>(); }

  void init(wgpu::Device device, render_targets::target_pool &pool,
            resources::handle<resources::lit_color_attachment> lit_target) {
    _pool = &pool;
    _lit_target = lit_target;

    _shader =
        shaders::render_shader::create_from_path(RESOURCE_DIR "/deferred_lighting.wgsl", device);
    _bindings = bindings::group::create();

    _position_input = bindings::borrowed_texture_view::create();
    _position_input->set_id(0);
    _position_input->set_frag_unfilterable_float_2d();

    _normal_input = bindings::borrowed_texture_view::create();
    _normal_input->set_id(1);
    _normal_input->set_frag_unfilterable_float_2d();

    _albedo_input = bindings::borrowed_texture_view::create();
    _albedo_input->set_id(2);
    _albedo_input->set_frag_unfilterable_float_2d();

    _ssao_input = bindings::borrowed_texture_view::create();
    _ssao_input->set_id(3);
    _ssao_input->set_frag_unfilterable_float_2d();

    _sampler = bindings::default_nearest_sampler(wgpu::ShaderStage::Fragment, device);
    _sampler->set_id(4);

    _bindings->append(_position_input);
    _bindings->append(_normal_input);
    _bindings->append(_albedo_input);
    _bindings->append(_ssao_input);
    _bindings->append(_sampler);

    _pipeline_ready = false;
  }

  void resize(wgpu::Device device) {
    _shader =
        shaders::render_shader::create_from_path(RESOURCE_DIR "/deferred_lighting.wgsl", device);
    _pipeline_ready = false;
  }

  void set_input(input, const resources::texture_input<resources::position_sampled> &position) {
    _position = position;
  }

  void set_input(input, const resources::texture_input<resources::normal_sampled> &normal) {
    _normal = normal;
  }

  void set_input(input, const resources::texture_input<resources::albedo_spec_sampled> &albedo) {
    _albedo = albedo;
  }

  void set_input(input, const resources::texture_input<resources::ssao_sampled> &ssao) {
    _ssao = ssao;
  }

  resources::handle<resources::lit_color_attachment> output() const { return _lit_target; }

  void set_pool(render_targets::target_pool &pool) { _pool = &pool; }

  std::vector<resource_use> resource_uses() const override {
    std::vector<resource_use> uses;
    if (_position.source.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _position.source.id});
    }
    if (_normal.source.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _normal.source.id});
    }
    if (_albedo.source.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _albedo.source.id});
    }
    if (_ssao.source.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _ssao.source.id});
    }
    if (_lit_target.id != render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _lit_target.id});
    }
    return uses;
  }

  void render(frame_context &ctx) override {
    if (!_pool || !_shader || _lit_target.id == render_targets::invalid_target_id) {
      return;
    }

    wgpu::Device device = ctx.device;
    ensure_pipeline(device);
    if (!_pipeline_ready) {
      return;
    }

    _position_input->set_view(_pool->get(_position.source.id).view());
    _normal_input->set_view(_pool->get(_normal.source.id).view());
    _albedo_input->set_view(_pool->get(_albedo.source.id).view());
    _ssao_input->set_view(_pool->get(_ssao.source.id).view());
    _bindings->init(device);

    render_targets::frame_attachments attachments{};
    attachments.color_targets = {_lit_target.id};

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
                       resources::lit_color_attachment::format(),
                       wgpu::TextureFormat::Undefined, "vs_main", "fs_main")) {
      _pipeline_ready = false;
      return;
    }
    _pipeline_ready = _shader->render_pipe_line() != nullptr;
  }

  render_targets::target_pool *_pool = nullptr;
  resources::handle<resources::lit_color_attachment> _lit_target{};
  resources::texture_input<resources::position_sampled> _position{};
  resources::texture_input<resources::normal_sampled> _normal{};
  resources::texture_input<resources::albedo_spec_sampled> _albedo{};
  resources::texture_input<resources::ssao_sampled> _ssao{};
  bindings::group::ptr _bindings;
  bindings::borrowed_texture_view::ptr _position_input;
  bindings::borrowed_texture_view::ptr _normal_input;
  bindings::borrowed_texture_view::ptr _albedo_input;
  bindings::borrowed_texture_view::ptr _ssao_input;
  bindings::sampler::ptr _sampler;
  shaders::render_shader::ptr _shader;
  bool _pipeline_ready = false;
};

} // namespace nodes
} // namespace lewitt
