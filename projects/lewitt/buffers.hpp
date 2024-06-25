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

      template <typename FORMAT>
      static ptr create()
      {
        return std::make_shared<buffer>(sizeof(FORMAT));
      }

      buffer(size_t size) : _sizeof_format(size) {}

      ~buffer()
      {
        _vertexBuffer.destroy();
        _vertexBuffer.release();
        _vertexCount = 0;
      }

      bool init(size_t vertex_count, wgpu::Device &device)
      {
        // Load mesh data from OBJ file
        // Create vertex buffer
        wgpu::BufferDescriptor bufferDesc;

        if (!_label.empty())
          bufferDesc.label = _label.c_str();

        bufferDesc.size = vertex_count * _sizeof_format;
        bufferDesc.usage = _usage;
        bufferDesc.mappedAtCreation = false;

        _vertexBuffer = device.createBuffer(bufferDesc);
        return _vertexBuffer != nullptr;
      }

      template <typename FORMAT>
      bool write(const std::vector<FORMAT> &vertexData, wgpu::Device &device)
      {
        if (!_vertexBuffer)
          init(vertexData.size(), device);

        wgpu::Queue queue = device.getQueue();
        _vertexCount = vertexData.size();
        queue.writeBuffer(_vertexBuffer, 0, vertexData.data(), size());
        _vertexCount = static_cast<int>(vertexData.size());
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
      size_t count() { return _vertexCount; }
      size_t size() { return _vertexCount * _sizeof_format; }

      void set_label(const std::string &label) { _label = label; }
      void set_usage(const WGPUBufferUsageFlags &usage) { _usage = usage; }

      std::string _label = "";
      WGPUBufferUsageFlags _usage = buffer_flags::vertex_read;

      wgpu::Buffer _vertexBuffer = nullptr;
      size_t _sizeof_format = 0;
      size_t _vertexCount = 0;
      // material::ptr _material;
    };

    // don't love how it relies on device and queue...
    inline buffer::ptr load_cylinder(wgpu::Device &device)
    {
      wgpu::Queue queue = device.getQueue();
      buffer::ptr buffer = buffer::create<lewitt::vertex_formats::NCUVTB_t>();

      std::vector<lewitt::vertex_formats::NCUVTB_t> vertexData =
          load_obj<lewitt::vertex_formats::NCUVTB_t>(RESOURCE_DIR "/cylinder.obj");

      bool success = vertexData.size() > 0;
      // success |= buffer->init(vertexData.size(), device);
      success |= buffer->write(vertexData, device);
      return buffer;
    }

  }
}