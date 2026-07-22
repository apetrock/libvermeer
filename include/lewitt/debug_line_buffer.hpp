#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <tuple>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "lewitt/buffer_flags.h"
#include "lewitt/buffers.hpp"
#include "lewitt/draw_primitives.hpp"
#include "lewitt/performance.hpp"

namespace lewitt {

class debug_line_buffer {
public:
  using ptr = std::shared_ptr<debug_line_buffer>;

  struct line {
    glm::vec3 p0{};
    glm::vec3 p1{};
    glm::vec3 color{1.0f, 0.0f, 0.0f};
    float radius = 0.01f;
  };

  static ptr create() { return std::make_shared<debug_line_buffer>(); }

  void ensure_static_geometry(wgpu::Device device) {
    LEWITT_PERF_SCOPE_PATH("lewitt::debug_line_buffer::ensure_static_geometry");
    if (_static_ready) {
      return;
    }

    auto [vertices, normals, indices, flags] =
        primitives::egg(64, 32, 0.5f, 0.5f, 0.0f);

    _position_buffer = buffers::buffer::create<glm::vec3>(
        vertices, device, flags::vertex::read);
    _position_buffer->set_vertex_layout<glm::vec3>(wgpu::VertexStepMode::Vertex);

    _normal_buffer = buffers::buffer::create<glm::vec3>(
        normals, device, flags::vertex::read);
    _normal_buffer->set_vertex_layout<glm::vec3>(wgpu::VertexStepMode::Vertex);

    _index_buffer = buffers::buffer::create<uint32_t>(
        indices, device, flags::index::read);

    _flag_buffer = buffers::buffer::create<uint32_t>(
        flags, device, flags::vertex::read);
    _flag_buffer->set_vertex_layout<uint32_t>(wgpu::VertexStepMode::Vertex);

    _radius_buffer = buffers::buffer::create();
    _radius_buffer->set_usage(flags::vertex::read);
    _radius_buffer->set_vertex_layout<float>(wgpu::VertexStepMode::Instance);

    _p0_buffer = buffers::buffer::create();
    _p0_buffer->set_usage(flags::vertex::read);
    _p0_buffer->set_vertex_layout<glm::vec3>(wgpu::VertexStepMode::Instance);

    _p1_buffer = buffers::buffer::create();
    _p1_buffer->set_usage(flags::vertex::read);
    _p1_buffer->set_vertex_layout<glm::vec3>(wgpu::VertexStepMode::Instance);

    _color_buffer = buffers::buffer::create();
    _color_buffer->set_usage(flags::vertex::read);
    _color_buffer->set_vertex_layout<glm::vec3>(wgpu::VertexStepMode::Instance);

    _position_buffer->set_format_offset(0);
    _normal_buffer->set_format_offset(1);
    _flag_buffer->set_format_offset(2);
    _radius_buffer->set_format_offset(3);
    _p0_buffer->set_format_offset(4);
    _p1_buffer->set_format_offset(5);
    _color_buffer->set_format_offset(6);

    _index_count = static_cast<uint32_t>(_index_buffer->count());
    _static_ready = true;
  }

  void clear() {
    if (_lines.empty() && !_valid && _instance_count == 0) {
      return;
    }
    _lines.clear();
    _instance_count = 0;
    _valid = false;
    _dirty = true;
  }

  void set_lines(const std::vector<line> &lines) {
    if (lines_equal(_lines, lines)) {
      _instance_count = static_cast<uint32_t>(_lines.size());
      _valid = _instance_count > 0;
      return;
    }
    _lines = lines;
    _instance_count = static_cast<uint32_t>(_lines.size());
    _valid = _instance_count > 0;
    _dirty = true;
  }

  void add_line(const glm::vec3 &p0, const glm::vec3 &p1,
                const glm::vec3 &color = glm::vec3(1.0f, 0.0f, 0.0f),
                float radius = 0.01f) {
    _lines.push_back({p0, p1, color, radius});
    _instance_count = static_cast<uint32_t>(_lines.size());
    _valid = _instance_count > 0;
    _dirty = true;
  }

  void upload(wgpu::Device device) {
    LEWITT_PERF_SCOPE_PATH("lewitt::debug_line_buffer::upload");
    ensure_static_geometry(device);
    if (!_valid || _lines.empty()) {
      return;
    }
    if (!_dirty) {
      return;
    }

    std::vector<glm::vec3> p0;
    std::vector<glm::vec3> p1;
    std::vector<glm::vec3> colors;
    std::vector<float> radii;
    p0.reserve(_lines.size());
    p1.reserve(_lines.size());
    colors.reserve(_lines.size());
    radii.reserve(_lines.size());
    for (const auto &entry : _lines) {
      p0.push_back(entry.p0);
      p1.push_back(entry.p1);
      colors.push_back(entry.color);
      radii.push_back(entry.radius);
    }

    _p0_buffer->write<glm::vec3>(p0, device);
    _p1_buffer->write<glm::vec3>(p1, device);
    _color_buffer->write<glm::vec3>(colors, device);
    _radius_buffer->write<float>(radii, device);
    _dirty = false;
  }

  bool valid() const { return _valid && _instance_count > 0 && _static_ready; }
  uint32_t instance_count() const { return _instance_count; }
  uint32_t index_count() const { return _index_count; }

  const buffers::buffer::ptr &position_buffer() const { return _position_buffer; }
  const buffers::buffer::ptr &normal_buffer() const { return _normal_buffer; }
  const buffers::buffer::ptr &flag_buffer() const { return _flag_buffer; }
  const buffers::buffer::ptr &radius_buffer() const { return _radius_buffer; }
  const buffers::buffer::ptr &p0_buffer() const { return _p0_buffer; }
  const buffers::buffer::ptr &p1_buffer() const { return _p1_buffer; }
  const buffers::buffer::ptr &color_buffer() const { return _color_buffer; }
  const buffers::buffer::ptr &index_buffer() const { return _index_buffer; }

  std::vector<wgpu::VertexBufferLayout> vertex_layouts() const {
    return {
        _position_buffer->get_vertex_layout(),
        _normal_buffer->get_vertex_layout(),
        _flag_buffer->get_vertex_layout(),
        _radius_buffer->get_vertex_layout(),
        _p0_buffer->get_vertex_layout(),
        _p1_buffer->get_vertex_layout(),
        _color_buffer->get_vertex_layout(),
    };
  }

private:
  static bool lines_equal(const std::vector<line> &a, const std::vector<line> &b) {
    if (a.size() != b.size()) {
      return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
      const auto &lhs = a[i];
      const auto &rhs = b[i];
      if (std::tie(lhs.p0.x, lhs.p0.y, lhs.p0.z, lhs.p1.x, lhs.p1.y, lhs.p1.z,
                   lhs.color.x, lhs.color.y, lhs.color.z, lhs.radius) !=
          std::tie(rhs.p0.x, rhs.p0.y, rhs.p0.z, rhs.p1.x, rhs.p1.y, rhs.p1.z,
                   rhs.color.x, rhs.color.y, rhs.color.z, rhs.radius)) {
        return false;
      }
    }
    return true;
  }

  std::vector<line> _lines;
  buffers::buffer::ptr _position_buffer;
  buffers::buffer::ptr _normal_buffer;
  buffers::buffer::ptr _flag_buffer;
  buffers::buffer::ptr _radius_buffer;
  buffers::buffer::ptr _p0_buffer;
  buffers::buffer::ptr _p1_buffer;
  buffers::buffer::ptr _color_buffer;
  buffers::buffer::ptr _index_buffer;
  uint32_t _index_count = 0;
  uint32_t _instance_count = 0;
  bool _static_ready = false;
  bool _valid = false;
  bool _dirty = true;
};

} // namespace lewitt
