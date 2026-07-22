#pragma once

#include <functional>
#include <memory>
#include <variant>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "lewitt/bindings.hpp"
#include "lewitt/frame_context.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/performance.hpp"
#include "lewitt/present_target.hpp"
#include "lewitt/resource_handles.hpp"
#include "lewitt/shaders.hpp"
#include "lewitt/texture_target.hpp"
#include "liblombardi/node_base.hpp"
#include "mondrian/draw_bundles.hpp"
#include "mondrian/draw_schema.hpp"

namespace lombardi {
namespace nodes {

enum class resource_use_kind { read, write, present };

struct resource_use {
  resource_use_kind kind = resource_use_kind::read;
  lewitt::render_targets::target_id resource = lewitt::render_targets::invalid_target_id;
};

class render_node : public liblombardi::Node {
public:
  ~render_node() override = default;

  uint port_count() const override { return 0; }

  void compute() override {
    if (_frame_ctx) {
      render(*_frame_ctx);
    }
  }

  virtual void render(lewitt::frame_context &ctx) = 0;
  virtual bool terminal_sink() const { return false; }
  virtual std::vector<resource_use> resource_uses() const { return {}; }
  virtual const lewitt::render_targets::target_pool *texture_pool() const { return nullptr; }

  void set_frame_context(lewitt::frame_context *ctx) { _frame_ctx = ctx; }

protected:
  lewitt::frame_context *_frame_ctx = nullptr;
};

enum class sink_display_mode { position, normal, color, ssao, depth };
using passthrough_mode = sink_display_mode;

class swapchain_sink : public render_node {
public:
  using ptr = std::shared_ptr<swapchain_sink>;
  using source_input =
      std::variant<lewitt::resources::texture_input<lewitt::resources::position_sampled>,
                   lewitt::resources::texture_input<lewitt::resources::normal_sampled>,
                   lewitt::resources::texture_input<lewitt::resources::color_sampled>,
                   lewitt::resources::texture_input<lewitt::resources::ssao_sampled>>;

  enum class input_port { source };
  enum class output_port { present };

  static ptr create() { return std::make_shared<swapchain_sink>(); }

  void init(wgpu::Device device, const lewitt::present_target &target) {
    _present_target = target;
    _device = device;
    rebuild_bindings(device);
    _bindings_ready = false;
    _pipeline_state.clear();
  }

  void resize(wgpu::Device device) {
    _device = device;
    rebuild_bindings(device);
    _bindings_ready = false;
    _pipeline_state.clear();
  }

  void set_present_target(const lewitt::present_target &target) { _present_target = target; }

  void set_input(input_port,
                 const lewitt::resources::texture_input<lewitt::resources::position_sampled> &input) {
    _source = input;
    _mode = sink_display_mode::position;
    if (_device) {
      rebuild_bindings(_device);
      _bindings_ready = false;
      _pipeline_state.clear();
    }
  }

  void set_input(input_port,
                 const lewitt::resources::texture_input<lewitt::resources::normal_sampled> &input) {
    _source = input;
    _mode = sink_display_mode::normal;
    if (_device) {
      rebuild_bindings(_device);
      _bindings_ready = false;
      _pipeline_state.clear();
    }
  }

  void set_input(input_port,
                 const lewitt::resources::texture_input<lewitt::resources::color_sampled> &input) {
    _source = input;
    _mode = sink_display_mode::color;
    if (_device) {
      rebuild_bindings(_device);
      _bindings_ready = false;
      _pipeline_state.clear();
    }
  }

  void set_input(input_port,
                 const lewitt::resources::texture_input<lewitt::resources::ssao_sampled> &input) {
    _source = input;
    _mode = sink_display_mode::ssao;
    if (_device) {
      rebuild_bindings(_device);
      _bindings_ready = false;
      _pipeline_state.clear();
    }
  }

  void set_mode(sink_display_mode mode) {
    _mode = mode;
    if (_device) {
      rebuild_bindings(_device);
      _bindings_ready = false;
      _pipeline_state.clear();
    }
  }

  bool terminal_sink() const override { return true; }

  std::vector<resource_use> resource_uses() const override {
    const lewitt::render_targets::target_id source_id = active_source_id();
    if (source_id == lewitt::render_targets::invalid_target_id) {
      return {};
    }
    return {{resource_use_kind::read, source_id}};
  }

  void draw(wgpu::RenderPassEncoder &enc, wgpu::Device &device, wgpu::Queue &queue,
            const lewitt::render_targets::target_pool &pool) {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::swapchain_sink::draw");
    const lewitt::render_targets::target_id source_id = active_source_id();
    if (source_id == lewitt::render_targets::invalid_target_id || !pool.valid(source_id)) {
      return;
    }

    if (_input->set_view(pool.get(source_id).view())) {
      _bindings->mark_dirty();
    }

    if (_mode == sink_display_mode::position && _display_params) {
      _display_params->set_member("mode", static_cast<uint32_t>(0));
      _display_params->update(queue);
    } else if (_mode == sink_display_mode::normal && _display_params) {
      _display_params->set_member("mode", static_cast<uint32_t>(1));
      _display_params->update(queue);
    }

    ensure_bindings(device);
    auto pipeline = _pipeline_state.get_or_create(device, _schema, _bindings);
    if (!pipeline) {
      return;
    }

    mondrian::binding_bundle bindings{};
    bindings.groups = {_bindings};
    mondrian::record_fullscreen(enc, pipeline, bindings);
  }

  void render(lewitt::frame_context &ctx) override {
    // Terminal sinks are presented by render_graph with overlay support.
  }

  void present(lewitt::frame_context &ctx,
               const std::function<void(wgpu::RenderPassEncoder &)> &overlay,
               const lewitt::render_targets::target_pool &sample_pool) {
    LEWITT_PERF_SCOPE_PATH("lombardi::nodes::swapchain_sink::present");
    if (!_present_target.swapchain && ctx.swapchain) {
      _present_target.swapchain = ctx.swapchain;
    }

    lewitt::render_targets::frame_attachments attachments{};
    lewitt::passes::render(_present_target.swapchain, ctx.device, attachments,
                           [&](wgpu::RenderPassEncoder &enc, wgpu::Device &dev) {
                             draw(enc, dev, ctx.queue, sample_pool);
                             if (overlay) {
                               overlay(enc);
                             }
                           },
                           true);
  }

private:
  lewitt::render_targets::target_id active_source_id() const {
    if (const auto *position = std::get_if<
            lewitt::resources::texture_input<lewitt::resources::position_sampled>>(&_source)) {
      return position->source.id;
    }
    if (const auto *normal = std::get_if<
            lewitt::resources::texture_input<lewitt::resources::normal_sampled>>(&_source)) {
      return normal->source.id;
    }
    if (const auto *color =
            std::get_if<lewitt::resources::texture_input<lewitt::resources::color_sampled>>(
                &_source)) {
      return color->source.id;
    }
    if (const auto *ssao =
            std::get_if<lewitt::resources::texture_input<lewitt::resources::ssao_sampled>>(
                &_source)) {
      return ssao->source.id;
    }
    return lewitt::render_targets::invalid_target_id;
  }

  void ensure_bindings(wgpu::Device device) {
    if (_bindings_ready || !_bindings) {
      return;
    }
    _bindings->init_layout(device);
    _bindings->init(device);
    _bindings_ready = true;
  }

  void rebuild_bindings(wgpu::Device device) {
    switch (_mode) {
    case sink_display_mode::position:
    case sink_display_mode::normal:
      _schema = mondrian::fullscreen_schema(RESOURCE_DIR "/gbuffer_display.wgsl",
                                            _present_target.format);
      break;
    case sink_display_mode::ssao:
      _schema = mondrian::fullscreen_schema(
          RESOURCE_DIR "/texture_grayscale_passthrough.wgsl", _present_target.format);
      break;
    case sink_display_mode::color:
    case sink_display_mode::depth:
    default:
      _schema = mondrian::fullscreen_schema(RESOURCE_DIR "/texture_passthrough.wgsl",
                                            _present_target.format);
      break;
    }

    _bindings = lewitt::bindings::group::create();
    _input = lewitt::bindings::borrowed_texture_view::create();
    _input->set_id(0);
    if (_mode == sink_display_mode::ssao) {
      _input->set_frag_unfilterable_float_2d();
      _sampler = lewitt::bindings::default_nearest_sampler(wgpu::ShaderStage::Fragment, device);
    } else {
      _input->set_frag_float_2d();
      _sampler = lewitt::bindings::default_linear_filter(wgpu::ShaderStage::Fragment, device);
    }
    _sampler->set_id(1);
    _bindings->append(_input);
    _bindings->append(_sampler);

    _display_params.reset();
    if (_mode == sink_display_mode::position || _mode == sink_display_mode::normal) {
      _display_params = lewitt::bindings::uniform::create<uint32_t, uint32_t, uint32_t, uint32_t>(
          {"mode", "_pad0", "_pad1", "_pad2"}, device);
      _display_params->set_visibility(wgpu::ShaderStage::Fragment);
      _display_params->set_id(2);
      _bindings->append(_display_params);
    }
  }

  bool _bindings_ready = false;
  sink_display_mode _mode = sink_display_mode::position;
  lewitt::present_target _present_target{};
  wgpu::Device _device = nullptr;
  source_input _source{};
  mondrian::draw_schema _schema{};
  mondrian::pipeline_state _pipeline_state;
  lewitt::bindings::group::ptr _bindings;
  lewitt::bindings::borrowed_texture_view::ptr _input;
  lewitt::bindings::sampler::ptr _sampler;
  lewitt::bindings::uniform::ptr _display_params;
};

using passthrough_visualizer = swapchain_sink;

class node_pool {
public:
  void add(const std::shared_ptr<render_node> &node) { _nodes.push_back(node); }

  void render(lewitt::frame_context &ctx) {
    for (const auto &node : _nodes) {
      if (node) {
        node->render(ctx);
      }
    }
  }

  void clear() { _nodes.clear(); }

private:
  std::vector<std::shared_ptr<render_node>> _nodes;
};

} // namespace nodes
} // namespace lombardi
