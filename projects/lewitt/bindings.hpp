
#pragma once
#include <stack>

#include <webgpu/webgpu.hpp>

#include "common.h"
#include "ResourceManager.h"
#include "uniforms.hpp"
#include "buffers.hpp"
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

      virtual void update(wgpu::Queue &queue) {}

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
        _uniforms.set<T>(mem, val);
        __update_queue.push({offset, size});
      }

      virtual void update(wgpu::Queue &queue)
      {

        if (_w_max > 0)
        {
          size_t size = _w_max - _w_min;
          std::cout << "  uniform buffer update: " << _w_min << " " << _w_max << " " << _uniforms.size() << std::endl;
          __update_queue = std::stack<std::array<size_t, 2>>();
          // queue.writeBuffer(_buffer, _w_min, static_cast<const char *>(_uniforms.data()), size);
          queue.writeBuffer(_buffer, 0, static_cast<const char *>(_uniforms.data()), _uniforms.size());
          _w_min = _uniforms.size();
          _w_max = 0;
          std::cout << "done!" << std::endl;
        }
      }

      virtual void add_to_layout(std::vector<wgpu::BindGroupLayoutEntry> &entries) override
      {
        // The normal map binding
        std::cout << " uniform" << std::endl;
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

    class buffer : public binding
    {
    public:
      using ptr = std::shared_ptr<buffer>;
      template <typename T>
      static ptr create()
      {
        ptr b = std::make_shared<buffer>();
        b->_buffer = buffers::buffer::create<T>();
        return b;
      }

      buffer()
      {
      }

      ~buffer()
      {
      }
      buffers::buffer::ptr get_buffer() { return _buffer; }
      bool valid()
      {
        return _buffer != nullptr;
      }

      template <typename T>
      void write(const std::vector<T> &data, wgpu::Device device)
      {
        _buffer->write<T>(data, device);
      }

      virtual void add_to_layout(std::vector<wgpu::BindGroupLayoutEntry> &entries)
      {
        // The normal map binding
        // std::cout << " texture" << std::endl;
        // wgpu::BindGroupLayoutEntry &textBindingLayout = entries[_id];
        // textBindingLayout.binding = _id;
        /// textBindingLayout.visibility = _visibility;
        // std::cout << "type 1: " << _sample_type << std::endl;
        // std::cout << "dim 1: " << _dim << std::endl;
        // textBindingLayout.texture.sampleType = _sample_type;
        // textBindingLayout.texture.viewDimension = _dim;
      }

      virtual void add_to_group(std::vector<wgpu::BindGroupEntry> &bindings)
      {
        // this->binding::add_to_group(bindings);
        // bindings[this->_id].textureView = _texture_view;
      }
      buffers::buffer::ptr _buffer;
    };

    class texture : public binding
    {
    public:
      DEFINE_CREATE_FUNC(texture);
      texture(std::pair<wgpu::Texture, wgpu::TextureView> tex) : _texture(tex.first), _texture_view(tex.second)
      {
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
      void set_sample_type(WGPUTextureSampleType type)
      {
        _sample_type = type;
      }
      void set_dimension(WGPUTextureViewDimension dim)
      {
        _dim = dim;
      }

      void set_frag_float_2d()
      {
        this->set_visibility(wgpu::ShaderStage::Fragment);
        this->set_sample_type(wgpu::TextureSampleType::Float);
        this->set_dimension(wgpu::TextureViewDimension::_2D);
      }

      virtual void add_to_layout(std::vector<wgpu::BindGroupLayoutEntry> &entries)
      {
        // The normal map binding
        std::cout << " texture" << std::endl;
        wgpu::BindGroupLayoutEntry &textBindingLayout = entries[_id];
        textBindingLayout.binding = _id;
        textBindingLayout.visibility = _visibility;
        std::cout << "type 1: " << _sample_type << std::endl;
        std::cout << "dim 1: " << _dim << std::endl;
        textBindingLayout.texture.sampleType = _sample_type;
        textBindingLayout.texture.viewDimension = _dim;
      }

      virtual void add_to_group(std::vector<wgpu::BindGroupEntry> &bindings)
      {
        this->binding::add_to_group(bindings);
        bindings[this->_id].textureView = _texture_view;
      }

      virtual wgpu::Texture get_texture()
      {
        return _texture;
      }

      WGPUTextureViewDimension _dim;
      WGPUTextureSampleType _sample_type;
      wgpu::Texture _texture = nullptr;
      wgpu::TextureView _texture_view = nullptr;
    };

    class storage_texture : public texture
    {
    public:
      DEFINE_CREATE_FUNC(storage_texture);
      storage_texture(std::pair<wgpu::Texture, wgpu::TextureView> tex) : _texture(tex.first), _texture_view(tex.second)
      {
      }

      ~storage_texture()
      {
        _texture_view.release();
        _texture.destroy();
        _texture.release();
      }

      bool valid()
      {
        return _texture != nullptr;
      }
      void set_sample_type(WGPUTextureSampleType type)
      {
        _sample_type = type;
      }
      void set_dimension(WGPUTextureViewDimension dim)
      {
        _dim = dim;
      }

      virtual void add_to_layout(std::vector<wgpu::BindGroupLayoutEntry> &entries)
      {
        // The normal map binding
        std::cout << " texture" << std::endl;
        wgpu::BindGroupLayoutEntry &textBindingLayout = entries[_id];
        textBindingLayout.binding = _id;
        textBindingLayout.visibility = _visibility;
        std::cout << "type 1: " << _sample_type << std::endl;
        std::cout << "dim 1: " << _dim << std::endl;
        textBindingLayout.storageTexture.access = StorageTextureAccess::WriteOnly;
        textBindingLayout.storageTexture.format = TextureFormat::RGBA8Unorm;
        textBindingLayout.storageTexture.viewDimension = TextureViewDimension::_2D;
      }

      virtual wgpu::Texture get_texture()
      {
        return _texture;
      }
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
        std::cout << " sampler" << std::endl;
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

    inline sampler::ptr default_linear_filter(const wgpu::ShaderStage &stage, wgpu::Device &device)
    {
      // convenience function for getting a linear filter
      wgpu::SamplerDescriptor samplerDesc;
      samplerDesc.addressModeU = wgpu::AddressMode::Repeat;
      samplerDesc.addressModeV = wgpu::AddressMode::Repeat;
      samplerDesc.addressModeW = wgpu::AddressMode::Repeat;
      samplerDesc.magFilter = wgpu::FilterMode::Linear;
      samplerDesc.minFilter = wgpu::FilterMode::Linear;
      samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
      samplerDesc.lodMinClamp = 0.0f;
      samplerDesc.lodMaxClamp = 8.0f;
      samplerDesc.compare = wgpu::CompareFunction::Undefined;
      samplerDesc.maxAnisotropy = 1;

      lewitt::bindings::sampler::ptr sampler_binding = lewitt::bindings::sampler::create(samplerDesc, device);
      sampler_binding->set_type(wgpu::SamplerBindingType::Filtering);
      sampler_binding->set_visibility(stage);
      return sampler_binding;
    }

    class group
    {
    public:
      DEFINE_CREATE_FUNC(group);

      group() {}
      ~group()
      {
        _layout.release();
        _group.release();
      }

      template <typename T>
      typename T::ptr get_binding(int id)
      {
        return std::dynamic_pointer_cast<T>(_bindings[id]);
      }

      uniform::ptr get_uniform_binding(int id)
      {
        return get_binding<uniform>(id);
      }

      int append(const binding::ptr &binding)
      {
        _bindings.push_back(binding);
        return _bindings.size() - 1;
      }

      int insert(const int &i, const binding::ptr &binding)
      {
        if (_bindings.size() < i + 1)
          _bindings.resize(i + 1, nullptr);

        std::vector<binding::ptr>::iterator it = _bindings.begin();
        std::vector<binding::ptr>::iterator end = _bindings.end();

        _bindings.insert(it + i, binding);
      }

      bool init_layout(wgpu::Device &device)
      {
        std::vector<wgpu::BindGroupLayoutEntry> bindingLayoutEntries(_bindings.size(), wgpu::Default);
        for (int i = 0; i < _bindings.size(); i++)
        {
          std::cout << "i: " << i << std::endl;
          _bindings[i]->set_id(i);
          _bindings[i]->add_to_layout(bindingLayoutEntries);
        }
        wgpu::BindGroupLayoutDescriptor bindGroupLayoutDesc{};
        bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
        bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
        _layout = device.createBindGroupLayout(bindGroupLayoutDesc);

        return _layout != nullptr;
      }

      bool init(wgpu::Device &device)
      {
        std::vector<wgpu::BindGroupEntry> bindings(_bindings.size());
        for (int i = 0; i < _bindings.size(); i++)
        {
          _bindings[i]->add_to_group(bindings);
        }

        wgpu::BindGroupDescriptor bindGroupDesc;
        bindGroupDesc.layout = _layout;
        bindGroupDesc.entryCount = (uint32_t)bindings.size();
        bindGroupDesc.entries = bindings.data();
        _group = device.createBindGroup(bindGroupDesc);

        return _group != nullptr;
      }

      wgpu::BindGroup &get_group()
      {
        return _group;
      }

      wgpu::BindGroupLayout &get_layout()
      {
        return _layout;
      }

      wgpu::BindGroup _group = nullptr;
      wgpu::BindGroupLayout _layout = nullptr;
      std::vector<binding::ptr> _bindings;
    };
  }
}