#pragma once
#include <stack>
#include "common.h"
#include <webgpu/webgpu.hpp>
#include "resources.hpp"
#include "vertex_formats.hpp"
#include "buffer_flags.h"
#include "passes.hpp"
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
      bool success = resources::loadGeometryFromObj(filename, vertexData);
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

      static ptr create(size_t count, size_t size, wgpu::Device &device, WGPUBufferUsageFlags usage = flags::storage::write)
      {
        ptr n_buff = std::make_shared<buffer>();
        n_buff->set_usage(usage);
        std::cout << "count: " << count << " elem_size: " << size << std::endl;
        n_buff->init(count, size, device);
        
        return n_buff;
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
        if (!_vertexBuffer)
          return;
        _vertexBuffer.destroy();
        _vertexBuffer.release();
        _count = 0;
      }

      bool init(size_t count, size_t size, wgpu::Device &device)
      {
        wgpu::BufferDescriptor bufferDesc;
        _sizeof_format = size;
        _count = count;
        if (!_label.empty())
          bufferDesc.label = _label.c_str();

        bufferDesc.size = count * _sizeof_format;
        bufferDesc.size = (bufferDesc.size + 3) & ~0x3; // must be multiples of 4
        bufferDesc.usage = _usage;
        bufferDesc.mappedAtCreation = false;

        _vertexBuffer = device.createBuffer(bufferDesc);

        return _vertexBuffer != nullptr;
      }

      template <typename FORMAT>
      bool init(size_t count, wgpu::Device &device)
      {
        return init(count, sizeof(FORMAT), device);
      }

      template <typename FORMAT>
      bool write(const std::vector<FORMAT> &vertexData, wgpu::Device &device)
      {
        _sizeof_format = sizeof(FORMAT);

        if (_vertexBuffer && _count < vertexData.size())
        {
          _vertexBuffer.destroy();
          _vertexBuffer = nullptr;
        }

        if (!_vertexBuffer)
          init(vertexData.size(), _sizeof_format, device);

        wgpu::Queue queue = device.getQueue();
        _count = static_cast<size_t>(vertexData.size());

        queue.writeBuffer(_vertexBuffer, 0, vertexData.data(), size());
        return _vertexBuffer != nullptr;
      }

      template <typename FORMAT>
      std::vector<FORMAT> read(wgpu::Device &device){
        
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
        for (int i = 0; i < _vertex_format.size(); i++)
        {
          _vertex_format[i].shaderLocation = i + offset;
        }
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
      size_t format_size() { return _sizeof_format; }

      void set_label(const std::string &label) { _label = label; }
      void set_usage(const WGPUBufferUsageFlags &usage) { _usage = usage; }

      std::string _label = "";
      WGPUBufferUsageFlags _usage = flags::vertex::read;

      wgpu::Buffer _vertexBuffer = nullptr;
      size_t _sizeof_format = 0;
      size_t _count = 0;

      vertex_formats::Format _vertex_format;
      wgpu::VertexBufferLayout _vertex_layout;
    };

    // move this to draw_primitives... or some other name
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
          resources::load_geometry_from_obj<lewitt::vertex_formats::PN_t>(RESOURCE_DIR "/bunny.obj");

      buffer::ptr attr_buffer = buffer::create<lewitt::vertex_formats::PN_t>(attributes, device, flags::vertex::read);
      buffer::ptr index_buffer = buffer::create<uint32_t>(indices, device, flags::index::read);
      attr_buffer->set_vertex_layout<vec3, vec3>(wgpu::VertexStepMode::Vertex);
      return {index_buffer, attr_buffer};
    }
  }
}