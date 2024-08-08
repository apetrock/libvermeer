/*
 *  manifold_singleton.h
 *  Phase Vocoder
 *
 *  Created by John Delaney on 12/29/10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */
#pragma once
#include "doables.hpp"
#include <iostream>

namespace lewitt {

/*
    class lineable : public renderable
    {
      lineable()
      {
      }
      ~lineable()
      {
      }

      void init(wgpu::Device device)
      {
        auto [vertices, normals, indices, flags] = lewitt::primitives::egg(64, 32, 0.5, 0.5, 0.0);

        lewitt::buffers::buffer::ptr vert_buffer =
            lewitt::buffers::buffer::create<vec3>(vertices, device, lewitt::flags::vertex::read);
        vert_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Vertex);

        lewitt::buffers::buffer::ptr norm_buffer =
            lewitt::buffers::buffer::create<vec3>(normals, device, lewitt::flags::vertex::read);
        norm_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Vertex);

        lewitt::buffers::buffer::ptr index_buffer =
            lewitt::buffers::buffer::create<uint32_t>(indices, device, lewitt::flags::index::read);

        lewitt::buffers::buffer::ptr flag_buffer =
            lewitt::buffers::buffer::create<uint32_t>(flags, device, lewitt::flags::vertex::read);
        flag_buffer->set_vertex_layout<uint32_t>(wgpu::VertexStepMode::Vertex);

        this->set_vertex_buffer(vert_buffer);
        this->set_index_buffer(index_buffer);
        this->set_shader(lewitt::shaders::shader_t::create(RESOURCE_DIR "/line.wgsl", m_device));

        this->append_attribute_buffer(norm_buffer);
        this->append_attribute_buffer(flag_buffer);
      }

      void add_line()
      {

        lewitt::buffers::buffer::ptr r_buffer =
            lewitt::buffers::buffer::create<float>({0.25}, m_device,
                                                   lewitt::flags::vertex::read);
        r_buffer->set_vertex_layout<float>(wgpu::VertexStepMode::Instance);
        capsule->append_attribute_buffer(r_buffer);

        lewitt::buffers::buffer::ptr p0_buffer =
            lewitt::buffers::buffer::create<vec3>({vec3(-1.0, -1.0, -1.0)}, m_device,
                                                  lewitt::flags::vertex::read);
        p0_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);
        capsule->append_attribute_buffer(p0_buffer);

        lewitt::buffers::buffer::ptr p1_buffer =
            lewitt::buffers::buffer::create<vec3>({vec3(1.0, 1.0, 1.0)}, m_device,
                                                  lewitt::flags::vertex::read);
        p1_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);
        capsule->append_attribute_buffer(p1_buffer);

        lewitt::buffers::buffer::ptr offset_attr_buffer =
            lewitt::buffers::buffer::create<vec3>({vec3(0.0, 0.0, 0.0)}, m_device,
                                                  lewitt::flags::vertex::read);
        offset_attr_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);
        capsule->append_attribute_buffer(offset_attr_buffer);

        lewitt::buffers::buffer::ptr color_attr_buffer =
            lewitt::buffers::buffer::create<vec3>({vec3(0.0, 1.0, 0.0)}, m_device,
                                                  lewitt::flags::vertex::read);
        color_attr_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);

        // right now need to copy the buffer, but should work, probably should work around
        // this location assignment so that we can reuse buffers
        capsule->append_attribute_buffer(color_attr_buffer);
        //_sphere->append_attribute_buffer(norm_buffer);

        lewitt::buffers::buffer::ptr quat_attr_buffer =
            lewitt::buffers::buffer::create<quat>({quat(0.0, 0.0, 0.0, 1.0)}, m_device,
                                                  lewitt::flags::vertex::read);
        quat_attr_buffer->set_vertex_layout<quat>(wgpu::VertexStepMode::Instance);
        capsule->append_attribute_buffer(quat_attr_buffer);

        capsule->set_instance_count(offset_attr_buffer->count());

        capsule->set_texture_format(m_swapChainFormat, m_depthTextureFormat);
      }

      void prep_buffers(wgpu::Device device){

      } 

      void draw(wgpu::RenderPassEncoder renderpass, wgpu::Device device)
      {

      }

    };
*/


class geometry_logger;

enum PresetColor {
  grey,
  red,
  green,
  blue,
  rainbow,
};

inline Vec4d rainbow4(double d) {
  double r = 0.5 + 0.5 * cos(2.0 * M_PI * (d + 0.000));
  double g = 0.5 + 0.5 * cos(2.0 * M_PI * (d + 0.333));
  double b = 0.5 + 0.5 * cos(2.0 * M_PI * (d + 0.666));
  return Vec4d(r, g, b, 1.0);
}

inline Vec4d sdf4(double d) {
  Vec4d inside(0.0, 1.0, 0.0, 1.0);
  Vec4d outside(1.0, 0.0, 0.0, 1.0);
  if (d < 0)
    return abs(d) * inside;
  else
    return abs(d) * outside;
}

class geometry_logger {

public:
  gg::DebugBufferPtr debugLines = NULL;

  static geometry_logger &get_instance();
  static void render();
  static void clear();
  static void point(const Vec3d &p0, const Vec4d &color);
  static void line4(const Vec4 &p0, const Vec4 &p1, const Vec4 &color);
  static void line(const Vec3d &p0, const Vec3d &p1, const Vec4d &color);

  static void box(const Vec3d &cen, const Vec3d &h, const Vec4d &col);
  static void ext(const Vec3d &mn, const Vec3d &mx, const Vec4d &col);

  static void lines(const std::vector<Vec3d> &p0, const std::vector<Vec3d> &p1,
                    const std::vector<Vec4d> &colors);
  static void field(const std::vector<Vec3d> &p, const std::vector<Vec3d> &dirs,
                    double D = 0.1, PresetColor col = grey);

  static void frame(Mat3d M, Vec3d c, double C);

  bool &initialized() { return instance_flag; }
  bool initialized() const { return instance_flag; }

private:
  geometry_logger() {
    global_instance = this;

    debugLines = gg::DebugBuffer::create();
    debugLines->init();
  }

  geometry_logger(const geometry_logger &);
  geometry_logger &operator=(const geometry_logger &);

  static geometry_logger *global_instance;
  static bool instance_flag;
};
} // namespace gg

#endif
