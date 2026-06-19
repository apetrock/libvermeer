#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <webgpu/webgpu.hpp>

#include "lewitt/buffer_flags.h"
#include "lewitt/buffers.hpp"
#include "lewitt/vertex_formats.hpp"

namespace lewitt {

class mesh_buffer {
public:
  using ptr = std::shared_ptr<mesh_buffer>;

  struct vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 color;
  };

  static ptr create() { return std::make_shared<mesh_buffer>(); }

  mesh_buffer() {
    _vertex_buffer = buffers::buffer::create();
    _vertex_buffer->set_usage(flags::vertex::read);
    _vertex_buffer->set_vertex_layout<glm::vec3, glm::vec3, glm::vec3>(
        wgpu::VertexStepMode::Vertex);

    _index_buffer = buffers::buffer::create();
    _index_buffer->set_usage(flags::index::read);
  }

  void set_data(const std::vector<vertex> &vertices,
                const std::vector<uint32_t> &indices, wgpu::Device device) {
    if (vertices.empty() || indices.empty()) {
      _valid = false;
      _index_count = 0;
      return;
    }
    _vertex_buffer->write<vertex>(vertices, device);
    _index_buffer->write<uint32_t>(indices, device);
    _index_count = static_cast<uint32_t>(_index_buffer->count());
    _valid = true;
  }

  void clear() {
    _valid = false;
    _index_count = 0;
  }

  bool valid() const { return _valid && _index_count > 0; }
  uint32_t index_count() const { return _index_count; }

  const buffers::buffer::ptr &vertex_buffer() const { return _vertex_buffer; }
  const buffers::buffer::ptr &index_buffer() const { return _index_buffer; }

  wgpu::VertexBufferLayout vertex_layout() const {
    return _vertex_buffer->get_vertex_layout();
  }

  static wgpu::VertexBufferLayout layout() {
    static lewitt::vertex_formats::Format attributes;
    static wgpu::VertexBufferLayout vertex_layout{};
    static bool initialized = false;
    if (!initialized) {
      auto [format, layout] =
          lewitt::vertex_formats::create_vertex_layout<glm::vec3, glm::vec3, glm::vec3>(
              wgpu::VertexStepMode::Vertex);
      attributes = std::move(format);
      vertex_layout = layout;
      vertex_layout.attributes = attributes.data();
      initialized = true;
    }
    return vertex_layout;
  }

private:
  buffers::buffer::ptr _vertex_buffer;
  buffers::buffer::ptr _index_buffer;
  uint32_t _index_count = 0;
  bool _valid = false;
};

} // namespace lewitt
