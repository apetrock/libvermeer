#pragma once

#include <memory>

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

class deferred_lighting_node : public render_node {
public:
  using ptr = std::shared_ptr<deferred_lighting_node>;

  enum class input { position, normal, albedo_spec, ssao };
  enum class output { lit_color };

  static ptr create() { return std::make_shared<deferred_lighting_node>(); }

  void init(wgpu::Device device, lewitt::render_targets::target_pool &pool,
            lewitt::resources::handle<lewitt::resources::lit_color_attachment> lit_target) {
    _pool = &pool;
    _lit_target = lit_target;

    _bindings = lewitt::bindings::group::create();

    _position_input = lewitt::bindings::borrowed_texture_view::create();
    _position_input->set_id(0);
    _position_input->set_frag_unfilterable_float_2d();

    _normal_input = lewitt::bindings::borrowed_texture_view::create();
    _normal_input->set_id(1);
    _normal_input->set_frag_unfilterable_float_2d();

    _albedo_input = lewitt::bindings::borrowed_texture_view::create();
    _albedo_input->set_id(2);
    _albedo_input->set_frag_unfilterable_float_2d();

    _ssao_input = lewitt::bindings::borrowed_texture_view::create();
    _ssao_input->set_id(3);
    _ssao_input->set_frag_unfilterable_float_2d();

    _sampler = lewitt::bindings::default_nearest_sampler(wgpu::ShaderStage::Fragment, device);
    _sampler->set_id(4);

    _bindings->append(_position_input);
    _bindings->append(_normal_input);
    _bindings->append(_albedo_input);
    _bindings->append(_ssao_input);
    _bindings->append(_sampler);

    _schema = mondrian::fullscreen_schema(RESOURCE_DIR "/deferred_lighting.wgsl",
                                          lewitt::resources::lit_color_attachment::format());
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

  void set_input(input,
                 const lewitt::resources::texture_input<lewitt::resources::albedo_spec_sampled> &albedo) {
    _albedo = albedo;
  }

  void set_input(input, const lewitt::resources::texture_input<lewitt::resources::ssao_sampled> &ssao) {
    _ssao = ssao;
  }

  lewitt::resources::handle<lewitt::resources::lit_color_attachment> output() const {
    return _lit_target;
  }

  void set_pool(lewitt::render_targets::target_pool &pool) { _pool = &pool; }

  std::vector<resource_use> resource_uses() const override {
    std::vector<resource_use> uses;
    if (_position.source.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _position.source.id});
    }
    if (_normal.source.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _normal.source.id});
    }
    if (_albedo.source.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _albedo.source.id});
    }
    if (_ssao.source.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::read, _ssao.source.id});
    }
    if (_lit_target.id != lewitt::render_targets::invalid_target_id) {
      uses.push_back({resource_use_kind::write, _lit_target.id});
    }
    return uses;
  }

  void render(lewitt::frame_context &ctx) override {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::deferred_lighting_node::render");
    if (!_pool || _lit_target.id == lewitt::render_targets::invalid_target_id) {
      return;
    }

    wgpu::Device device = ctx.device;
    ensure_bindings(device);

    bool bindings_changed = false;
    bindings_changed |= _position_input->set_view(_pool->get(_position.source.id).view());
    bindings_changed |= _normal_input->set_view(_pool->get(_normal.source.id).view());
    bindings_changed |= _albedo_input->set_view(_pool->get(_albedo.source.id).view());
    bindings_changed |= _ssao_input->set_view(_pool->get(_ssao.source.id).view());
    if (bindings_changed) {
      _bindings->mark_dirty();
    }
    _bindings->init(device);

    auto pipeline = _pipeline_state.get_or_create(device, _schema, _bindings);
    if (!pipeline) {
      return;
    }

    lewitt::render_targets::frame_attachments attachments{};
    attachments.color_targets = {_lit_target.id};

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
  lewitt::resources::handle<lewitt::resources::lit_color_attachment> _lit_target{};
  lewitt::resources::texture_input<lewitt::resources::position_sampled> _position{};
  lewitt::resources::texture_input<lewitt::resources::normal_sampled> _normal{};
  lewitt::resources::texture_input<lewitt::resources::albedo_spec_sampled> _albedo{};
  lewitt::resources::texture_input<lewitt::resources::ssao_sampled> _ssao{};
  lewitt::bindings::group::ptr _bindings;
  lewitt::bindings::borrowed_texture_view::ptr _position_input;
  lewitt::bindings::borrowed_texture_view::ptr _normal_input;
  lewitt::bindings::borrowed_texture_view::ptr _albedo_input;
  lewitt::bindings::borrowed_texture_view::ptr _ssao_input;
  lewitt::bindings::sampler::ptr _sampler;
  mondrian::draw_schema _schema{};
  mondrian::pipeline_state _pipeline_state;
  bool _bindings_ready = false;
};

} // namespace nodes
} // namespace lombardi
