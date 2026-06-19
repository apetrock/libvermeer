#pragma once

#include "lewitt/render_targets.hpp"
#include "lewitt/resource_handles.hpp"

namespace lewitt {

struct color_attachment_ref {
  enum class kind { owned, borrowed };

  kind type = kind::owned;
  render_targets::target_id owned_id = render_targets::invalid_target_id;
  render_targets::external_color_attachment borrowed{};
  render_targets::attachment_state state{};
};

namespace texture_target {

inline color_attachment_ref owned(render_targets::target_id id) {
  color_attachment_ref ref{};
  ref.type = color_attachment_ref::kind::owned;
  ref.owned_id = id;
  return ref;
}

inline color_attachment_ref borrow(render_targets::external_color_attachment attachment) {
  color_attachment_ref ref{};
  ref.type = color_attachment_ref::kind::borrowed;
  ref.borrowed = std::move(attachment);
  return ref;
}

inline resources::texture_output<resources::swapchain_color_attachment>
as_output(const color_attachment_ref &ref) {
  resources::texture_output<resources::swapchain_color_attachment> out{};
  if (ref.type == color_attachment_ref::kind::owned) {
    out.target.id = ref.owned_id;
  }
  return out;
}

} // namespace texture_target
} // namespace lewitt
