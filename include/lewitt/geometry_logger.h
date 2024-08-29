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
#include "draw_primitives.hpp"

#include <iostream>

namespace lewitt
{

  namespace doables
  {
    class lineable : public renderable
    {
    public:
      DEFINE_CREATE_FUNC(lineable);

      lineable()
      {
      }
      ~lineable()
      {
      }

      void init(wgpu::Device device)
      {
        if (_init)
          return;

        std::cout << "init line" << std::endl;
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
        this->set_shader(lewitt::shaders::shader_t::create(RESOURCE_DIR "/line.wgsl", device));

        this->append_attribute_buffer(norm_buffer);
        this->append_attribute_buffer(flag_buffer);

        init_instance_buffers(device);
        _init = true;
      }

      void init_instance_buffers(wgpu::Device device)
      {
        std::cout << "init instance buffers" << std::endl;
        _p0_buffer = lewitt::buffers::buffer::create();
        _p1_buffer = lewitt::buffers::buffer::create();
        _color_buffer = lewitt::buffers::buffer::create();
        _r_buffer = lewitt::buffers::buffer::create();
        _r_buffer->set_vertex_layout<float>(wgpu::VertexStepMode::Instance);
        _p0_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);
        _p1_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);
        _color_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);
        append_attribute_buffer(_r_buffer);
        append_attribute_buffer(_p0_buffer);
        append_attribute_buffer(_p1_buffer);
        append_attribute_buffer(_color_buffer);
        set_instance_count(0);
      }

      void clear()
      {
        _p0.clear();
        _p1.clear();
        _r.clear();
        _color.clear();
      }

      void add_line(const vec3x2 &line, const vec3 color = vec3(1.0, 0.0, 0.0), const float r = 1.0)
      {
        _p0.push_back(line[0]);
        _p1.push_back(line[1]);
        _r.push_back(r);
        _color.push_back(color);
      }

      void prep_buffers(wgpu::Device device)
      {
        _p0_buffer->write<vec3>(_p0, device);
        _p1_buffer->write<vec3>(_p1, device);
        _color_buffer->write<vec3>(_color, device);
        _r_buffer->write<float>(_r, device);
      }

      void draw(wgpu::RenderPassEncoder renderpass, wgpu::Device device)

      {
        init(device);

        this->set_instance_count(_p0_buffer->count());
        prep_buffers(device);
        renderable::draw(renderpass, device);
      }
      bool _init = false;

      std::vector<vec3> _p0;
      std::vector<vec3> _p1;
      std::vector<vec3> _color;
      std::vector<float> _r;

      lewitt::buffers::buffer::ptr _p0_buffer;
      lewitt::buffers::buffer::ptr _p1_buffer;
      lewitt::buffers::buffer::ptr _r_buffer;
      lewitt::buffers::buffer::ptr _color_buffer;
    };
  }

  namespace logger
  {
    class geometry
    {

    public:
      doables::lineable::ptr debugLines = NULL;

      static geometry &get_instance();
      static void point(const vec3 & p, const vec3 &color, const float & r);
      static void line(const vec3x2 & line, const vec3 &color, const float & r);

      bool &initialized() { return instance_flag; }
      bool initialized() const { return instance_flag; }

    private:
      geometry()
      {
        global_instance = this;

        debugLines = doables::lineable::create();
      }

      geometry(const geometry &);
      geometry &operator=(const geometry &);

      static geometry *global_instance;
      static bool instance_flag;
    };
  };
}
/*
class geometry_logger;

*/
// namespace gg
