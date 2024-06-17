
#pragma once
#include <stack>

#include <webgpu/webgpu.hpp>

#include "common.h"
#include "ResourceManager.h"
#include "uniforms.hpp"

// there will have to be scene uniforms and buffer uniforms,
// I think we can seperate all of those out.
namespace lewitt
{
  namespace bindings
  {

    GLM_TYPEDEFS;

    class binding
    {
    public:
      using ptr = std::shared_ptr<binding>;
      binding() {}
      void set_id(int id) { _id = id; }
      virtual void set_visibility(WGPUShaderStageFlags visibility)
      {
        _visibility = visibility;
      }

      virtual void add_to_layout(std::vector<wgpu::BindGroupLayoutEntry> &entries)
      {
      }

      virtual void add_to_group(std::vector<wgpu::BindGroupEntry> &bindings)
      {
        std::cout << "parent add to group" << std::endl;
        bindings[_id].binding = _id;
      }

      virtual bool valid()
      {
        return false;
      }

      WGPUShaderStageFlags _visibility;
      int _id = -1;
    };

    //
    //
    //

    struct LightingUniforms
    {
      std::array<vec4, 2> directions;
      std::array<vec4, 2> colors;

      // Material properties
      float hardness = 32.0f;
      float kd = 1.0f;
      float ks = 0.5f;

      float _pad[1];
    };

    class uniform : public binding
    {
    public:
      using ptr = std::shared_ptr<uniform>;
      template <typename... Types>
      static ptr create(std::initializer_list<std::string> names, wgpu::Device &device)
      {
        ptr out = std::make_shared<uniform>();
        out->init<Types...>(names, device);
        return out;
      }

      uniform()
      {
      }
      ~uniform()
      {
        _buffer.destroy();
        _buffer.release();
      }

      template <typename... Types>
      void init(std::initializer_list<std::string> names, wgpu::Device &device)
      {

        _uniforms.init<Types...>(names);
        _w_min = _uniforms.size();

        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = _uniforms.size();
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
        bufferDesc.mappedAtCreation = false;
        std::cout << "creating buffer: " << bufferDesc.size << std::endl;

        _buffer = device.createBuffer(bufferDesc);
      }

      template <typename T>
      void set_member(const std::string &mem, T val)
      {
        size_t offset = _uniforms.offset(mem);
        size_t size = sizeof(val);
        size_t c_min = offset;
        size_t c_max = offset + size;
        _w_min = std::min(_w_min, c_min);
        _w_max = std::max(_w_max, c_max);
        std::cout <<"  "<< mem << ": " << _w_min << " " << c_min << " " << _w_max << " " << _uniforms.size() << std::endl;
        _uniforms.set<T>(mem, val);
        __update_queue.push({offset, size});
      }

      void update(wgpu::Queue &queue)
      {

        if (_w_max > 0){
          size_t size = _w_max - _w_min;
          std::cout << "  uniform buffer update: "<< _w_min << " " << _w_max << " " << _uniforms.size() << std::endl;
          __update_queue = std::stack<std::array<size_t, 2>>();
          //queue.writeBuffer(_buffer, _w_min, static_cast<const char *>(_uniforms.data()), size);
          queue.writeBuffer(_buffer, 0, static_cast<const char *>(_uniforms.data()), _uniforms.size());
          _w_min = _uniforms.size();
          _w_max = 0;
          std::cout << "done!" << std::endl;
        }
      }

      void update(LightingUniforms u, wgpu::Queue &queue)
      {
        queue.writeBuffer(_buffer, 0, &u, sizeof(LightingUniforms));
      }

      virtual void add_to_layout(std::vector<wgpu::BindGroupLayoutEntry> &entries) override
      {
        // The normal map binding
        std::cout << "adding uniform to layout: " << _id << std::endl;
        wgpu::BindGroupLayoutEntry &layout = entries[_id];
        layout.binding = _id;
        layout.visibility = _visibility;
        layout.buffer.type = wgpu::BufferBindingType::Uniform;
        layout.buffer.minBindingSize = _uniforms.size();
      }

      virtual void add_to_group(std::vector<wgpu::BindGroupEntry> &bindings) override
      {
        std::cout << "adding buffer to group: " << _id << ", " << _buffer << std::endl;
        this->binding::add_to_group(bindings);
        bindings[_id].buffer = _buffer;
        bindings[_id].offset = 0;
        bindings[_id].size = _uniforms.size();
      }

      virtual bool valid()
      {
        return _buffer != nullptr;
      }

      uniforms::structish _uniforms;
      wgpu::Buffer _buffer = nullptr;
      size_t _w_min = 99999999, _w_max = 0;
      std::stack<std::array<size_t, 2>> __update_queue;
    };

    //
    //
    //

    class texture : public binding
    {
    public:
      DEFINE_CREATE_FUNC(texture);
      texture(std::string file_name,
              wgpu::Device &device)
      {
        _texture = ResourceManager::loadTexture(file_name, device, &_texture_view);
      }
      ~texture()
      {
        _texture_view.release();
        _texture.destroy();
        _texture.release();
      }
      bool valid()
      {
        return _texture != nullptr;
      }
      void set_sample_type(WGPUTextureSampleType type) { _sample_type = type; }
      void set_dimension(WGPUTextureViewDimension dim) { _dim = dim; }

      virtual void add_to_layout(std::vector<wgpu::BindGroupLayoutEntry> &entries)
      {
        // The normal map binding
        wgpu::BindGroupLayoutEntry &textBindingLayout = entries[_id];
        textBindingLayout.binding = _id;
        textBindingLayout.visibility = _visibility;
        textBindingLayout.texture.sampleType = _sample_type;
        textBindingLayout.texture.viewDimension = _dim;
      }

      virtual void add_to_group(std::vector<wgpu::BindGroupEntry> &bindings)
      {
        this->binding::add_to_group(bindings);
        bindings[this->_id].textureView = _texture_view;
      }

      WGPUTextureViewDimension _dim;
      WGPUTextureSampleType _sample_type;
      wgpu::Texture _texture = nullptr;
      wgpu::TextureView _texture_view = nullptr;
    };

    //
    //
    //

    class sampler : public binding
    {
    public:
      DEFINE_CREATE_FUNC(sampler);
      sampler(const wgpu::SamplerDescriptor &desc,
              wgpu::Device &device)
      {
        _sampler = device.createSampler(desc);
      }

      ~sampler()
      {
      }

      void set_type(WGPUSamplerBindingType type)
      {
        _type = type;
      }

      virtual void add_to_layout(std::vector<wgpu::BindGroupLayoutEntry> &entries)
      {
        // The normal map binding
        wgpu::BindGroupLayoutEntry &bindingLayout = entries[_id];
        bindingLayout.binding = _id;
        bindingLayout.visibility = _visibility;
        bindingLayout.sampler.type = _type;
      }

      virtual void add_to_group(std::vector<wgpu::BindGroupEntry> &bindings)
      {
        this->binding::add_to_group(bindings);
        bindings[this->_id].sampler = _sampler;
      }

      WGPUSamplerBindingType _type;
      wgpu::Sampler _sampler = nullptr;
    };
  }
}