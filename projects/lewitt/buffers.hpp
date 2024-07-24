#pragma once
#include <stack>
#include "common.h"
#include <webgpu/webgpu.hpp>
#include "ResourceManager.h"
#include "bindings.hpp"
#include "vertex_formats.hpp"
#include "buffer_flags.h"
// there will have to be scene uniforms and buffer uniforms,
// I think we can seperate all of those out.
namespace lewitt
{
  GLM_TYPEDEFS;
  namespace buffers
  {

    template <typename FORMAT>
    std::vector<FORMAT> load_obj(const std::string &filename)
    {
      // currently only have one format, this will have to improve
      std::vector<FORMAT> vertexData;
      bool success = ResourceManager::loadGeometryFromObj(filename, vertexData);
      if (!success)
      {
        std::cerr << "Could not load geometry!" << std::endl;
        return std::vector<FORMAT>();
      }
      return vertexData;
    }

    class buffer
    {
    public:
      using ptr = std::shared_ptr<buffer>;

      static ptr create()
      {
        return std::make_shared<buffer>();
      }
      template <typename FORMAT>
      static ptr create(
          const std::vector<FORMAT> &vertexData,
          wgpu::Device &device,
          WGPUBufferUsageFlags usage = flags::vertex::read)
      {
        auto buf = std::make_shared<buffer>();
        buf->set_usage(usage);
        // buf->init(vertexData.size(), device);
        if (buf->write<FORMAT>(vertexData, device))
          return buf;
        else
          return nullptr;
      }

      buffer() {}

      ~buffer()
      {
        _vertexBuffer.destroy();
        _vertexBuffer.release();
        _count = 0;
      }

      bool init(size_t count, wgpu::Device &device)
      {
        wgpu::BufferDescriptor bufferDesc;
        std::cout << "usage in init: " << _usage << std::endl;
        if (!_label.empty())
          bufferDesc.label = _label.c_str();

        bufferDesc.size = count * _sizeof_format;
        std::cout << "size before: " << bufferDesc.size << std::endl;
        bufferDesc.size = (bufferDesc.size + 3) & ~0x3; // must be multiples of 4
        std::cout << "size after: " << bufferDesc.size << std::endl;
        bufferDesc.usage = _usage;
        bufferDesc.mappedAtCreation = false;

        _vertexBuffer = device.createBuffer(bufferDesc);

        return _vertexBuffer != nullptr;
      }

      template <typename... Types>
      void set_vertex_layout(wgpu::VertexStepMode step_mode = wgpu::VertexStepMode::Vertex)
      {
        auto [vertex_format, vertex_layout] = vertex_formats::create_vertex_layout<Types...>(step_mode);
        _vertex_format = std::move(vertex_format);
        _vertex_layout = std::move(vertex_layout);
      }

      wgpu::VertexBufferLayout get_vertex_layout()
      {
        return _vertex_layout;
      }

      vertex_formats::Format get_vertex_format()
      {
        return _vertex_format;
      }

      void set_format_offset(uint32_t offset)
      {
        for(int i = 0; i < _vertex_format.size(); i++)
        {
          _vertex_format[i].shaderLocation = i + offset;
        }
      }

      template <typename FORMAT>
      bool write(const std::vector<FORMAT> &vertexData, wgpu::Device &device)
      {
        _sizeof_format = sizeof(FORMAT);

        if (!_vertexBuffer)
          init(vertexData.size(), device);

        wgpu::Queue queue = device.getQueue();
        _count = static_cast<size_t>(vertexData.size());

        queue.writeBuffer(_vertexBuffer, 0, vertexData.data(), size());
        return _vertexBuffer != nullptr;
      }

      bool valid()
      {
        return _vertexBuffer != nullptr;
      }

      wgpu::Buffer &get_buffer()
      {
        return _vertexBuffer;
      }
      size_t count() { return _count; }
      size_t size() { return _count * _sizeof_format; }

      void set_label(const std::string &label) { _label = label; }
      void set_usage(const WGPUBufferUsageFlags &usage) { _usage = usage; }

      std::string _label = "";
      WGPUBufferUsageFlags _usage = flags::vertex::read;

      wgpu::Buffer _vertexBuffer = nullptr;
      size_t _sizeof_format = 0;
      size_t _count = 0;

      // optional Format/layout ptrs.
      vertex_formats::Format _vertex_format;
      wgpu::VertexBufferLayout _vertex_layout;

      // material::ptr _material;
    };

    inline buffer::ptr load_cylinder(wgpu::Device &device)
    {
      wgpu::Queue queue = device.getQueue();

      std::vector<lewitt::vertex_formats::PNCUVTB_t> vertexData =
          load_obj<lewitt::vertex_formats::PNCUVTB_t>(RESOURCE_DIR "/cylinder.obj");

      buffer::ptr buffer = buffer::create(vertexData, device);
      return buffer;
    }

    inline std::array<buffer::ptr, 2> load_bunny(wgpu::Device &device)
    {
      auto [indices, attributes] =
          ResourceManager::load_geometry_from_obj<lewitt::vertex_formats::PN_t>(RESOURCE_DIR "/bunny.obj");

      buffer::ptr attr_buffer = buffer::create<lewitt::vertex_formats::PN_t>(attributes, device, flags::vertex::read);
      buffer::ptr index_buffer = buffer::create<uint32_t>(indices, device, flags::index::read);
      attr_buffer->set_vertex_layout<vec3, vec3>(wgpu::VertexStepMode::Vertex);
      return {index_buffer, attr_buffer};
    }

  }
}