#pragma once

#include <memory>

#include <glm/glm.hpp>
#include <webgpu/webgpu.hpp>

#include "lewitt/bindings.hpp"
#include "lewitt/frame_context.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/performance.hpp"
#include "lewitt/resource_handles.hpp"
#include "lombardi/nodes.hpp"
#include "mondrian/draw_bundles.hpp"
#include "mondrian/draw_schema.hpp"

namespace lombardi {
namespace nodes {

class ssao_node : public render_node {
public:
  using ptr = std::shared_ptr<ssao_node>;

  enum class input { position, normal, camera };
  enum class output { ao };

  static ptr create() { return std::make_shared<ssao_node>(); }

  void init(wgpu::Device device, lewitt::render_targets::target_pool &pool,
            lewitt::resources::handle<lewitt::resources::ssao_attachment> ao_target) {
    _pool = &pool;
    _ao_target = ao_target;

    _bindings = lewitt::bindings::group::create();

    _position_input = lewitt::bindings::borrowed_texture_view::create();
    _position_input->set_id(0);
    _position_input->set_frag_unfilterable_float_2d();

    _normal_input = lewitt::bindings::borrowed_texture_view::create();
    _normal_input->set_id(1);
    _normal_input->set_frag_unfilterable_float_2d();

    _sampler = lewitt::bindings::default_nearest_sampler(wgpu::ShaderStage::Fragment, device);
    _sampler->set_id(2);

    _params = lewitt::bindings::uniform::create<glm::mat4, glm::vec4>(
        {"projectionMatrix", "params"}, device);
    _params->set_visibility(wgpu::ShaderStage::Fragment);
    _params->set_id(3);
    // (radius_scale * |z|, bias, ao_power, fade_start) — softer contrast + distance fade
    _params->set_member("params", glm::vec4(0.02f, 0.01f, 1.75f, 8.0f));

    _bindings->append(_position_input);
    _bindings->append(_normal_input);
    _bindings->append(_sampler);
    _bindings->append(_params);

    _schema = mondrian::fullscreen_schema(RESOURCE_DIR "/ssao.wgsl",
                                          lewitt::resources::ssao_attachment::format());
    _bindings_ready = false;
    _pipeline_state.clear();
  }

  void resize(wgpu::Device device) {
    (void)device;
    _bindings_ready = false;
    _pipeline_state.clear();
  }

  void set_input(input, const lewitt::resources::texture_input<lewitt::resources::position_sampled> &position) {
    _position = position;
  }

  void set_input(input, const lewitt::resources::texture_input<lewitt::resources::normal_sampled> &normal) {
    _normal = normal;
  }

  void set_input(input, lewitt::bindings::uniform::ptr camera_uniform) {
    _camera_uniform = std::move(camera_uniform);
  }

  lewitt::resources::handle<lewitt::resources::ssao_attachment> output() const { return _ao_target; }

  void set_pool(lewitt::render_targets::target_pool &pool) { _pool = &pool; }

  std::vector<resource_use> resource_uses() const override {
    std::vector<resource_use> uses;
    if (_position.source.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _position.source.id});
    }
    if (_normal.source.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _normal.source.id});
    }
    if (_ao_target.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _ao_target.id});
    }
    return uses;
  }

  void render(lewitt::frame_context &ctx) override {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::ssao_node::render");
    if (!_pool || _ao_target.id == lewitt::render_targets::invalid_target_id) {
      return;
    }
    if (_position.source.id == lewitt::render_targets::invalid_target_id ||
        _normal.source.id == lewitt::render_targets::invalid_target_id) {
      return;
    }

    wgpu::Device device = ctx.device;
    ensure_bindings(device);

    if (_camera_uniform) {
      _params->set_member("projectionMatrix",
                          _camera_uniform->get_member<glm::mat4>("projectionMatrix"));
      _params->update(ctx.queue);
    }

    bool bindings_changed = false;
    bindings_changed |= _position_input->set_view(_pool->get(_position.source.id).view());
    bindings_changed |= _normal_input->set_view(_pool->get(_normal.source.id).view());
    if (bindings_changed) {
      _bindings->mark_dirty();
    }
    _bindings->init(device);

    auto pipeline = _pipeline_state.get_or_create(device, _schema, _bindings);
    if (!pipeline) {
      return;
    }

    lewitt::render_targets::frame_attachments attachments{};
    attachments.color_targets = {_ao_target.id};

    lewitt::passes::render(nullptr, device, *_pool, attachments,
                           [&](wgpu::RenderPassEncoder &enc, wgpu::Device &) {
                             mondrian::binding_bundle bindings{};
                             bindings.groups = {_bindings};
                             mondrian::record_fullscreen(enc, pipeline, bindings);
                           },
                           false);
  }

private:
  void ensure_bindings(wgpu::Device device) {
    if (_bindings_ready || !_bindings) {
      return;
    }
    _bindings->init_layout(device);
    _bindings_ready = true;
  }

  lewitt::render_targets::target_pool *_pool = nullptr;
  lewitt::resources::handle<lewitt::resources::ssao_attachment> _ao_target{};
  lewitt::resources::texture_input<lewitt::resources::position_sampled> _position{};
  lewitt::resources::texture_input<lewitt::resources::normal_sampled> _normal{};
  lewitt::bindings::uniform::ptr _camera_uniform;
  lewitt::bindings::uniform::ptr _params;
  lewitt::bindings::group::ptr _bindings;
  lewitt::bindings::borrowed_texture_view::ptr _position_input;
  lewitt::bindings::borrowed_texture_view::ptr _normal_input;
  lewitt::bindings::sampler::ptr _sampler;
  mondrian::draw_schema _schema{};
  mondrian::pipeline_state _pipeline_state;
  bool _bindings_ready = false;
};

} // namespace nodes
} // namespace lombardi
