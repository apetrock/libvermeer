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
      static ptr create(const std::vector<FORMAT> &vertexData, wgpu::Device &device)
      {
        auto buf = std::make_shared<buffer>();
        //buf->init(vertexData.size(), device);
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
        
        if (!_label.empty())
          bufferDesc.label = _label.c_str();

        bufferDesc.size = count * _sizeof_format;
        bufferDesc.usage = _usage;
        bufferDesc.mappedAtCreation = false;

        _vertexBuffer = device.createBuffer(bufferDesc);
        return _vertexBuffer != nullptr;
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

      void set_current(wgpu::RenderPassEncoder render_pass)
      {
        render_pass.setVertexBuffer(0, _vertexBuffer, 0, _vertexBuffer.getSize());
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
      // material::ptr _material;
    };

    inline buffer::ptr load_cylinder(wgpu::Device &device)
    {
      wgpu::Queue queue = device.getQueue();
      

      std::vector<lewitt::vertex_formats::NCUVTB_t> vertexData =
          load_obj<lewitt::vertex_formats::NCUVTB_t>(RESOURCE_DIR "/cylinder.obj");
      
      buffer::ptr buffer = buffer::create(vertexData, device);
      return buffer;
    }

    inline std::array<buffer::ptr, 2> load_bunny(wgpu::Device &device)
    {
      auto [indices, attributes] =
          ResourceManager::load_geometry_from_obj<lewitt::vertex_formats::N_t>(RESOURCE_DIR "/bunny.obj");
      buffer::ptr attr_buffer = buffer::create<lewitt::vertex_formats::N_t>(attributes, device);
      attr_buffer->set_usage(flags::vertex::read);
      buffer::ptr index_buffer = buffer::create<uint32_t>(indices, device);
      index_buffer->set_usage(flags::index::read);

      return {index_buffer, attr_buffer};
    }

  }
}