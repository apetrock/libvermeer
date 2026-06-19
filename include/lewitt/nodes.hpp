#pragma once

#include <functional>
#include <memory>
#include <variant>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "common.h"
#include "lewitt/bindings.hpp"
#include "lewitt/frame_context.hpp"
#include "lewitt/passes.hpp"
#include "lewitt/present_target.hpp"
#include "lewitt/resource_handles.hpp"
#include "lewitt/shaders.hpp"
#include "lewitt/texture_target.hpp"

namespace lewitt {
namespace nodes {

enum class resource_use_kind { read, write, present };

struct resource_use {
  resource_use_kind kind = resource_use_kind::read;
  render_targets::target_id resource = render_targets::invalid_target_id;
};

class render_node {
public:
  virtual ~render_node() = default;
  virtual void render(frame_context &ctx) = 0;
  virtual bool terminal_sink() const { return false; }
  virtual std::vector<resource_use> resource_uses() const { return {}; }
  virtual const render_targets::target_pool *texture_pool() const { return nullptr; }
};

enum class sink_display_mode { position, normal, color, ssao, depth };
using passthrough_mode = sink_display_mode;

class swapchain_sink : public render_node {
public:
  using ptr = std::shared_ptr<swapchain_sink>;
  using source_input =
      std::variant<resources::texture_input<resources::position_sampled>,
                   resources::texture_input<resources::normal_sampled>,
                   resources::texture_input<resources::color_sampled>,
                   resources::texture_input<resources::ssao_sampled>>;

  enum class input_port { source };
  enum class output_port { present };

  static ptr create() { return std::make_shared<swapchain_sink>(); }

  void init(wgpu::Device device, const present_target &target) {
    _present_target = target;
    _device = device;
    rebuild_bindings(device);
    _inited = false;
  }

  void resize(wgpu::Device device) {
    _device = device;
    rebuild_bindings(device);
    _inited = false;
  }

  void set_present_target(const present_target &target) { _present_target = target; }

  void set_input(input_port, const resources::texture_input<resources::position_sampled> &input) {
    _source = input;
    _mode = sink_display_mode::position;
    if (_device) {
      rebuild_bindings(_device);
      _inited = false;
    }
  }

  void set_input(input_port, const resources::texture_input<resources::normal_sampled> &input) {
    _source = input;
    _mode = sink_display_mode::normal;
    if (_device) {
      rebuild_bindings(_device);
      _inited = false;
    }
  }

  void set_input(input_port, const resources::texture_input<resources::color_sampled> &input) {
    _source = input;
    _mode = sink_display_mode::color;
    if (_device) {
      rebuild_bindings(_device);
      _inited = false;
    }
  }

  void set_input(input_port, const resources::texture_input<resources::ssao_sampled> &input) {
    _source = input;
    _mode = sink_display_mode::ssao;
    if (_device) {
      rebuild_bindings(_device);
      _inited = false;
    }
  }

  void set_mode(sink_display_mode mode) {
    _mode = mode;
    if (_device) {
      rebuild_bindings(_device);
      _inited = false;
    }
  }

  bool terminal_sink() const override { return true; }

  std::vector<resource_use> resource_uses() const override {
    const render_targets::target_id source_id = active_source_id();
    if (source_id == render_targets::invalid_target_id) {
      return {};
    }
    return {{resource_use_kind::read, source_id}};
  }

  void draw(wgpu::RenderPassEncoder &enc, wgpu::Device &device, wgpu::Queue &queue,
            const render_targets::target_pool &pool) {
    const render_targets::target_id source_id = active_source_id();
    if (source_id == render_targets::invalid_target_id || !pool.valid(source_id)) {
      return;
    }

    _input->set_view(pool.get(source_id).view());

    if (_mode == sink_display_mode::position && _display_params) {
      _display_params->set_member("mode", static_cast<uint32_t>(0));
      _display_params->update(queue);
    } else if (_mode == sink_display_mode::normal && _display_params) {
      _display_params->set_member("mode", static_cast<uint32_t>(1));
      _display_params->update(queue);
    }

    if (!_inited) {
      _bindings->init_layout(device);
      _bindings->init(device);
      _shader->init(device, _bindings->get_layout(), _present_target.format,
                    wgpu::TextureFormat::Undefined);
      _inited = true;
    } else {
      _bindings->init(device);
    }

    enc.setPipeline(_shader->render_pipe_line());
    enc.setBindGroup(0, _bindings->get_group(), 0, nullptr);
    enc.draw(3, 1, 0, 0);
  }

  void render(frame_context &ctx) override {
    // Terminal sinks are presented by render_graph with overlay support.
  }

  void present(frame_context &ctx, const std::function<void(wgpu::RenderPassEncoder &)> &overlay,
               const render_targets::target_pool &sample_pool) {
    if (!_present_target.swapchain && ctx.swapchain) {
      _present_target.swapchain = ctx.swapchain;
    }

    render_targets::frame_attachments attachments{};
    passes::render(_present_target.swapchain, ctx.device, attachments,
                   [&](wgpu::RenderPassEncoder &enc, wgpu::Device &dev) {
                     draw(enc, dev, ctx.queue, sample_pool);
                     if (overlay) {
                       overlay(enc);
                     }
                   },
                   true);
  }

private:
  render_targets::target_id active_source_id() const {
    if (const auto *position =
            std::get_if<resources::texture_input<resources::position_sampled>>(&_source)) {
      return position->source.id;
    }
    if (const auto *normal =
            std::get_if<resources::texture_input<resources::normal_sampled>>(&_source)) {
      return normal->source.id;
    }
    if (const auto *color =
            std::get_if<resources::texture_input<resources::color_sampled>>(&_source)) {
      return color->source.id;
    }
    if (const auto *ssao =
            std::get_if<resources::texture_input<resources::ssao_sampled>>(&_source)) {
      return ssao->source.id;
    }
    return render_targets::invalid_target_id;
  }

  void rebuild_bindings(wgpu::Device device) {
    switch (_mode) {
    case sink_display_mode::position:
    case sink_display_mode::normal:
      _shader = shaders::render_shader::create_from_path(RESOURCE_DIR "/gbuffer_display.wgsl",
                                                         device);
      break;
    case sink_display_mode::ssao:
      _shader = shaders::render_shader::create_from_path(
          RESOURCE_DIR "/texture_grayscale_passthrough.wgsl", device);
      break;
    case sink_display_mode::color:
    case sink_display_mode::depth:
    default:
      _shader = shaders::render_shader::create_from_path(RESOURCE_DIR "/texture_passthrough.wgsl",
                                                         device);
      break;
    }

    _bindings = bindings::group::create();
    _input = bindings::borrowed_texture_view::create();
    _input->set_id(0);
    if (_mode == sink_display_mode::ssao) {
      _input->set_frag_unfilterable_float_2d();
      _sampler = bindings::default_nearest_sampler(wgpu::ShaderStage::Fragment, device);
    } else {
      _input->set_frag_float_2d();
      _sampler = bindings::default_linear_filter(wgpu::ShaderStage::Fragment, device);
    }
    _sampler->set_id(1);
    _bindings->append(_input);
    _bindings->append(_sampler);

    _display_params.reset();
    if (_mode == sink_display_mode::position || _mode == sink_display_mode::normal) {
      _display_params = bindings::uniform::create<uint32_t, uint32_t, uint32_t, uint32_t>(
          {"mode", "_pad0", "_pad1", "_pad2"}, device);
      _display_params->set_visibility(wgpu::ShaderStage::Fragment);
      _display_params->set_id(2);
      _bindings->append(_display_params);
    }
  }

  bool _inited = false;
  sink_display_mode _mode = sink_display_mode::position;
  present_target _present_target{};
  wgpu::Device _device = nullptr;
  source_input _source{};
  shaders::render_shader::ptr _shader;
  bindings::group::ptr _bindings;
  bindings::borrowed_texture_view::ptr _input;
  bindings::sampler::ptr _sampler;
  bindings::uniform::ptr _display_params;
};

using passthrough_visualizer = swapchain_sink;

class node_pool {
public:
  void add(const std::shared_ptr<render_node> &node) { _nodes.push_back(node); }

  void render(frame_context &ctx) {
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
} // namespace lewitt
