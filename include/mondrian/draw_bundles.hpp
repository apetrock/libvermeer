#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "lewitt/bindings.hpp"
#include "lewitt/buffers.hpp"
#include "lewitt/debug_line_buffer.hpp"
#include "lewitt/debug_sphere_buffer.hpp"
#include "lewitt/debug_torus_buffer.hpp"
#include "lewitt/mesh_buffer.hpp"

namespace mondrian {

struct binding_bundle {
  std::vector<lewitt::bindings::group::ptr> groups;
};

struct indexed_draw_command {
  uint32_t index_count = 0;
  uint32_t instance_count = 1;
  uint32_t first_index = 0;
  int32_t base_vertex = 0;
};

struct fullscreen_draw_command {
  uint32_t vertex_count = 3;
  uint32_t instance_count = 1;
};

struct mesh_vertex_bundle {
  lewitt::buffers::buffer::ptr vertex_buffer;
  lewitt::buffers::buffer::ptr index_buffer;
  indexed_draw_command command{};
};

struct debug_line_vertex_bundle {
  std::array<lewitt::buffers::buffer::ptr, 7> vertex_buffers{};
  lewitt::buffers::buffer::ptr index_buffer;
  indexed_draw_command command{};
};

struct debug_sphere_vertex_bundle {
  std::array<lewitt::buffers::buffer::ptr, 5> vertex_buffers{};
  lewitt::buffers::buffer::ptr index_buffer;
  indexed_draw_command command{};
};

struct debug_torus_vertex_bundle {
  std::array<lewitt::buffers::buffer::ptr, 7> vertex_buffers{};
  lewitt::buffers::buffer::ptr index_buffer;
  indexed_draw_command command{};
};

inline void collect_bind_groups(const binding_bundle &bundle,
                                std::vector<lewitt::bindings::group::ptr> &out) {
  out.insert(out.end(), bundle.groups.begin(), bundle.groups.end());
}

inline void bind_vertices(const mesh_vertex_bundle &vertices, wgpu::RenderPassEncoder &enc) {
  if (!vertices.vertex_buffer || !vertices.index_buffer) {
    return;
  }
  enc.setVertexBuffer(0, vertices.vertex_buffer->get_buffer(), 0,
                      vertices.vertex_buffer->get_buffer().getSize());
  enc.setIndexBuffer(vertices.index_buffer->get_buffer(), wgpu::IndexFormat::Uint32, 0,
                     vertices.index_buffer->size());
}

inline void record_draw(const mesh_vertex_bundle &vertices, wgpu::RenderPassEncoder &enc) {
  if (!vertices.index_buffer || vertices.command.index_count == 0) {
    return;
  }
  enc.drawIndexed(vertices.command.index_count, vertices.command.instance_count,
                  vertices.command.first_index, vertices.command.base_vertex, 0);
}

inline void bind_vertices(const debug_line_vertex_bundle &vertices,
                          wgpu::RenderPassEncoder &enc) {
  for (uint32_t slot = 0; slot < vertices.vertex_buffers.size(); ++slot) {
    if (!vertices.vertex_buffers[slot]) {
      continue;
    }
    enc.setVertexBuffer(slot, vertices.vertex_buffers[slot]->get_buffer(), 0,
                        vertices.vertex_buffers[slot]->get_buffer().getSize());
  }
  if (vertices.index_buffer) {
    enc.setIndexBuffer(vertices.index_buffer->get_buffer(), wgpu::IndexFormat::Uint32, 0,
                       vertices.index_buffer->size());
  }
}

inline void record_draw(const debug_line_vertex_bundle &vertices, wgpu::RenderPassEncoder &enc) {
  if (!vertices.index_buffer || vertices.command.index_count == 0) {
    return;
  }
  enc.drawIndexed(vertices.command.index_count, vertices.command.instance_count,
                  vertices.command.first_index, vertices.command.base_vertex, 0);
}

inline void bind_vertices(const debug_sphere_vertex_bundle &vertices,
                          wgpu::RenderPassEncoder &enc) {
  for (uint32_t slot = 0; slot < vertices.vertex_buffers.size(); ++slot) {
    if (!vertices.vertex_buffers[slot]) {
      continue;
    }
    enc.setVertexBuffer(slot, vertices.vertex_buffers[slot]->get_buffer(), 0,
                        vertices.vertex_buffers[slot]->get_buffer().getSize());
  }
  if (vertices.index_buffer) {
    enc.setIndexBuffer(vertices.index_buffer->get_buffer(), wgpu::IndexFormat::Uint32, 0,
                       vertices.index_buffer->size());
  }
}

inline void record_draw(const debug_sphere_vertex_bundle &vertices,
                        wgpu::RenderPassEncoder &enc) {
  if (!vertices.index_buffer || vertices.command.index_count == 0) {
    return;
  }
  enc.drawIndexed(vertices.command.index_count, vertices.command.instance_count,
                  vertices.command.first_index, vertices.command.base_vertex, 0);
}

inline void bind_vertices(const debug_torus_vertex_bundle &vertices,
                          wgpu::RenderPassEncoder &enc) {
  for (uint32_t slot = 0; slot < vertices.vertex_buffers.size(); ++slot) {
    if (!vertices.vertex_buffers[slot]) {
      continue;
    }
    enc.setVertexBuffer(slot, vertices.vertex_buffers[slot]->get_buffer(), 0,
                        vertices.vertex_buffers[slot]->get_buffer().getSize());
  }
  if (vertices.index_buffer) {
    enc.setIndexBuffer(vertices.index_buffer->get_buffer(), wgpu::IndexFormat::Uint32, 0,
                       vertices.index_buffer->size());
  }
}

inline void record_draw(const debug_torus_vertex_bundle &vertices,
                        wgpu::RenderPassEncoder &enc) {
  if (!vertices.index_buffer || vertices.command.index_count == 0) {
    return;
  }
  enc.drawIndexed(vertices.command.index_count, vertices.command.instance_count,
                  vertices.command.first_index, vertices.command.base_vertex, 0);
}

inline void record_draw(const fullscreen_draw_command &command, wgpu::RenderPassEncoder &enc) {
  enc.draw(command.vertex_count, command.instance_count, 0, 0);
}

template <typename T>
concept BindGroupSource = requires(const T &value, std::vector<lewitt::bindings::group::ptr> &out) {
  { collect_bind_groups(value, out) } -> std::same_as<void>;
};

template <typename T>
concept VertexSource = requires(const T &value, wgpu::RenderPassEncoder &enc) {
  { bind_vertices(value, enc) } -> std::same_as<void>;
};

template <typename T>
concept DrawCommandSource = requires(const T &value, wgpu::RenderPassEncoder &enc) {
  { record_draw(value, enc) } -> std::same_as<void>;
};

template <typename T>
concept Drawable = VertexSource<T> && DrawCommandSource<T>;

template <BindGroupSource... Bundles>
void bind_all(wgpu::RenderPassEncoder &enc, const Bundles &...bundles) {
  std::vector<lewitt::bindings::group::ptr> groups;
  (collect_bind_groups(bundles, groups), ...);
  for (uint32_t group_index = 0; group_index < groups.size(); ++group_index) {
    if (!groups[group_index]) {
      continue;
    }
    enc.setBindGroup(group_index, groups[group_index]->get_group(), 0, nullptr);
  }
}

template <Drawable DrawableT, BindGroupSource BindingsT>
void record(wgpu::RenderPassEncoder &enc, wgpu::RenderPipeline pipeline,
            const DrawableT &drawable, const BindingsT &bindings) {
  enc.setPipeline(pipeline);
  bind_all(enc, bindings);
  bind_vertices(drawable, enc);
  record_draw(drawable, enc);
}

template <BindGroupSource BindingsT>
void record_fullscreen(wgpu::RenderPassEncoder &enc, wgpu::RenderPipeline pipeline,
                       const BindingsT &bindings,
                       const fullscreen_draw_command &command = {}) {
  enc.setPipeline(pipeline);
  bind_all(enc, bindings);
  record_draw(command, enc);
}

struct mesh_drawable {
  mesh_vertex_bundle vertices{};
  binding_bundle bindings{};
};

struct debug_line_drawable {
  debug_line_vertex_bundle vertices{};
  binding_bundle bindings{};
};

struct fullscreen_drawable {
  binding_bundle bindings{};
  fullscreen_draw_command command{};
};

inline mesh_vertex_bundle make_mesh_vertex_bundle(const lewitt::mesh_buffer &mesh) {
  mesh_vertex_bundle out{};
  if (!mesh.valid()) {
    return out;
  }
  out.vertex_buffer = mesh.vertex_buffer();
  out.index_buffer = mesh.index_buffer();
  out.command.index_count = mesh.index_count();
  out.command.instance_count = 1;
  return out;
}

inline debug_line_vertex_bundle make_debug_line_vertex_bundle(const lewitt::debug_line_buffer &lines) {
  debug_line_vertex_bundle out{};
  if (!lines.valid()) {
    return out;
  }
  out.vertex_buffers[0] = lines.position_buffer();
  out.vertex_buffers[1] = lines.normal_buffer();
  out.vertex_buffers[2] = lines.flag_buffer();
  out.vertex_buffers[3] = lines.radius_buffer();
  out.vertex_buffers[4] = lines.p0_buffer();
  out.vertex_buffers[5] = lines.p1_buffer();
  out.vertex_buffers[6] = lines.color_buffer();
  out.index_buffer = lines.index_buffer();
  out.command.index_count = lines.index_count();
  out.command.instance_count = lines.instance_count();
  return out;
}

inline debug_sphere_vertex_bundle
make_debug_sphere_vertex_bundle(const lewitt::debug_sphere_buffer &spheres) {
  debug_sphere_vertex_bundle out{};
  if (!spheres.valid()) {
    return out;
  }
  out.vertex_buffers[0] = spheres.position_buffer();
  out.vertex_buffers[1] = spheres.normal_buffer();
  out.vertex_buffers[2] = spheres.center_buffer();
  out.vertex_buffers[3] = spheres.radius_buffer();
  out.vertex_buffers[4] = spheres.color_buffer();
  out.index_buffer = spheres.index_buffer();
  out.command.index_count = spheres.index_count();
  out.command.instance_count = spheres.instance_count();
  return out;
}

inline debug_torus_vertex_bundle
make_debug_torus_vertex_bundle(const lewitt::debug_torus_buffer &tori) {
  debug_torus_vertex_bundle out{};
  if (!tori.valid()) {
    return out;
  }
  out.vertex_buffers[0] = tori.position_buffer();
  out.vertex_buffers[1] = tori.normal_buffer();
  out.vertex_buffers[2] = tori.center_buffer();
  out.vertex_buffers[3] = tori.axis_buffer();
  out.vertex_buffers[4] = tori.major_buffer();
  out.vertex_buffers[5] = tori.minor_buffer();
  out.vertex_buffers[6] = tori.color_buffer();
  out.index_buffer = tori.index_buffer();
  out.command.index_count = tori.index_count();
  out.command.instance_count = tori.instance_count();
  return out;
}

inline fullscreen_drawable make_fullscreen_drawable(lewitt::bindings::group::ptr bindings) {
  fullscreen_drawable out{};
  if (bindings) {
    out.bindings.groups.push_back(std::move(bindings));
  }
  return out;
}

} // namespace mondrian
