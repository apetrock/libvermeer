#pragma once

#include <type_traits>

#include <webgpu/webgpu.hpp>

#include "lewitt/render_targets.hpp"

namespace lewitt {
namespace resources {

enum class resource_role { attachment, sampled };

struct position_attachment {
  static constexpr resource_role role = resource_role::attachment;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA16Float; }
};

struct normal_attachment {
  static constexpr resource_role role = resource_role::attachment;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA16Float; }
};

struct albedo_spec_attachment {
  static constexpr resource_role role = resource_role::attachment;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA8Unorm; }
};

struct depth_attachment {
  static constexpr resource_role role = resource_role::attachment;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::Depth24Plus; }
};

struct position_sampled {
  static constexpr resource_role role = resource_role::sampled;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA16Float; }
};

struct normal_sampled {
  static constexpr resource_role role = resource_role::sampled;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA16Float; }
};

struct albedo_spec_sampled {
  static constexpr resource_role role = resource_role::sampled;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA8Unorm; }
};

struct ssao_attachment {
  static constexpr resource_role role = resource_role::attachment;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA16Float; }
};

struct ssao_sampled {
  static constexpr resource_role role = resource_role::sampled;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA16Float; }
};

struct lit_color_attachment {
  static constexpr resource_role role = resource_role::attachment;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA8Unorm; }
};

struct lit_color_sampled {
  static constexpr resource_role role = resource_role::sampled;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA8Unorm; }
};

struct color_sampled {
  static constexpr resource_role role = resource_role::sampled;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::RGBA8Unorm; }
};

struct swapchain_color_attachment {
  static constexpr resource_role role = resource_role::attachment;
  static wgpu::TextureFormat format() { return wgpu::TextureFormat::Undefined; }
};

template <typename Tag>
struct handle {
  render_targets::target_id id = render_targets::invalid_target_id;
};

template <typename Tag>
struct texture_input {
  handle<Tag> source;
};

template <typename Tag>
struct texture_output {
  handle<Tag> target;
};

struct g_buffer_outputs {
  handle<position_attachment> position;
  handle<normal_attachment> normal;
  handle<albedo_spec_attachment> albedo_spec;
  handle<depth_attachment> depth;
};

template <typename OutTag, typename InTag>
void wire(const handle<OutTag> &, handle<InTag> &) {
  static_assert(sizeof(OutTag) == 0,
                "no wire() overload for these resource handle types");
}

inline void wire(const handle<position_attachment> &out,
                 handle<position_sampled> &in) {
  in.id = out.id;
}

inline void wire(const handle<normal_attachment> &out, handle<normal_sampled> &in) {
  in.id = out.id;
}

inline void wire(const handle<albedo_spec_attachment> &out, handle<albedo_spec_sampled> &in) {
  in.id = out.id;
}

inline void wire(const handle<ssao_attachment> &out, handle<ssao_sampled> &in) {
  in.id = out.id;
}

inline void wire(const handle<lit_color_attachment> &out, handle<lit_color_sampled> &in) {
  in.id = out.id;
}

inline void wire(const handle<lit_color_attachment> &out, handle<color_sampled> &in) {
  in.id = out.id;
}

inline void wire(const handle<ssao_attachment> &out, handle<color_sampled> &in) {
  in.id = out.id;
}

inline render_targets::render_target_desc
color_attachment_desc(const char *label, wgpu::TextureFormat format) {
  render_targets::render_target_desc desc;
  desc.label = label;
  desc.format = format;
  desc.usage = wgpu::TextureUsage::RenderAttachment;
  desc.size_policy = render_targets::extent_policy::swapchain;
  desc.attachment = render_targets::attachment_state::color_clear();
  return desc;
}

inline render_targets::render_target_desc
sampleable_color_attachment_desc(const char *label, wgpu::TextureFormat format) {
  render_targets::render_target_desc desc = color_attachment_desc(label, format);
  desc.usage = wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
  return desc;
}

inline render_targets::sampled_texture_view
as_sampled(const render_targets::render_target &target) {
  render_targets::sampled_texture_view view{};
  view.view = target.view();
  view.sample_type = wgpu::TextureSampleType::Float;
  view.dimension = wgpu::TextureViewDimension::_2D;
  return view;
}

inline render_targets::attachment_view
as_attachment(const render_targets::render_target &target) {
  render_targets::attachment_view view{};
  view.view = target.view();
  view.state = target.desc().attachment;
  view.borrowed = false;
  return view;
}

inline render_targets::sampled_texture_view
resolve_sampled(const render_targets::target_pool &pool,
                const texture_input<position_sampled> &input) {
  if (!pool.valid(input.source.id)) {
    return {};
  }
  return as_sampled(pool.get(input.source.id));
}

inline render_targets::attachment_view
resolve_attachment(const render_targets::target_pool &pool,
                   const texture_output<position_attachment> &output) {
  if (!pool.valid(output.target.id)) {
    return {};
  }
  return as_attachment(pool.get(output.target.id));
}

} // namespace resources
} // namespace lewitt
