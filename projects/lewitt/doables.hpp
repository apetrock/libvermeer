#pragma once
#include <stack>
#include "common.h"
#include <webgpu/webgpu.hpp>
#include "ResourceManager.h"

#include "bindings.hpp"
#include "vertex_formats.hpp"
#include "shaders.hpp"
#include "buffers.hpp"
#include "buffer_flags.h"
// there will have to be scene uniforms and buffer uniforms,
// I think we can seperate all of those out.
namespace lewitt
{

  namespace doables
  {

    class doable
    {
    public:
      using ptr = std::shared_ptr<doable>;
      doable() {}
      ~doable() {}

      void set_bindings(const bindings::group::ptr &bindings)
      {
        _bindings = bindings;
      }
      void set_shader(const shaders::shader::ptr &shader)
      {
        _shader = shader;
      }
      bindings::group::ptr _bindings;
      shaders::shader::ptr _shader;
    };

    class renderable : public doable
    {
    public:
      using ptr = std::shared_ptr<renderable>;
      static renderable::ptr create()
      {
        ptr obj = std::make_shared<renderable>();
        return obj;
      }

      static renderable::ptr create(
          const buffers::buffer::ptr &vertex_buffer,
          const bindings::group::ptr &bindings,
          const shaders::shader::ptr &shader)
      {
        ptr obj = std::make_shared<renderable>();
        obj->set_bindings(bindings);
        obj->set_vertex_buffer(vertex_buffer);
        obj->set_shader(shader);
        return obj;
      }

      renderable()
      {
      }
      ~renderable() {}

      void draw(wgpu::RenderPassEncoder renderpass)
      {

        renderpass.setPipeline(_shader->render_pipe_line());
        /// renderPass.setVertexBuffer(0, m_vertexBuffer, 0, m_vertexCount * sizeof(VertexAttributes));
        vertex_buffer->set_current(renderpass);
        renderpass.setBindGroup(0, _bindings->get_group(), 0, nullptr);
        if (index_buffer)
        {
          renderpass.setIndexBuffer(index_buffer->get_buffer(), wgpu::IndexFormat::Uint32, 0, index_buffer->count());
          renderpass.drawIndexed(index_buffer->count(), 1, 0, 0, 0);
        }
        else
        {
          renderpass.draw(vertex_buffer->count(), 1, 0, 0);
        }
      }

      void set_vertex_buffer(const buffers::buffer::ptr &buffer)
      {
        vertex_buffer = buffer;
      }
      void set_index_buffer(const buffers::buffer::ptr &buffer){
        index_buffer = buffer;
      }

      buffers::buffer::ptr vertex_buffer = nullptr;
      buffers::buffer::ptr index_buffer = nullptr;
    };

    class computable : public doable
    {
    public:
      using ptr = std::shared_ptr<computable>;
      static computable::ptr create()
      {
        ptr obj = std::make_shared<computable>();
        return obj;
      }

      static computable::ptr create(
          const bindings::group::ptr &bindings,
          const shaders::shader::ptr &shader)
      {
        ptr obj = std::make_shared<computable>();
        obj->set_bindings(bindings);
        obj->set_shader(shader);
        return obj;
      }

      computable()
      {
      }
      ~computable() {}

      void compute(wgpu::ComputePassEncoder computePass)
      {
        std::cout << "setting pipeline: " << this->_shader->compute_pipe_line() << std::endl;
        computePass.setPipeline(this->_shader->compute_pipe_line());
        std::cout << " invocations: " << _invocation_count_x << "x" << _invocation_count_y << std::endl;
        for (uint32_t i = 0; i < 1; ++i)
        {
          computePass.setBindGroup(0, this->_bindings->get_group(), 0, nullptr);

          uint32_t invocationCountX = _invocation_count_x;
          uint32_t invocationCountY = _invocation_count_y;
          uint32_t workgroupSizePerDim = 8;
          // This ceils invocationCountX / workgroupSizePerDim
          uint32_t workgroupCountX = (invocationCountX + workgroupSizePerDim - 1) / workgroupSizePerDim;
          uint32_t workgroupCountY = (invocationCountY + workgroupSizePerDim - 1) / workgroupSizePerDim;

          computePass.dispatchWorkgroups(workgroupCountX, workgroupCountY, 1);
        }
      }

      void set_invocation_count(uint32_t x, uint32_t y)
      {
        _invocation_count_x = x;
        _invocation_count_y = y;
      }

      uint32_t _invocation_count_x = 0;
      uint32_t _invocation_count_y = 0;
    };

    class ray_compute : public computable
    {
    public:
      GLM_TYPEDEFS;

      DEFINE_CREATE_FUNC(ray_compute)

      ray_compute(wgpu::Device device)
      {
        this->_shader = ray_shader::create(device);
        _bindings = bindings::group::create();
      }

      ~ray_compute() {}

      struct ray
      {
        vec3 origin;
        uint32_t pad;
        vec3 direction;
        uint32_t id;
      };

      struct Uniforms
      {
        quat orientation = quat(1.0, 0.0, 0.0, 0.0);
        uint32_t width = 0;
        uint32_t height = 0;
        float p0;
        float p1;
      };

      static_assert(sizeof(ray) % 16 == 0);

      ray init_ray(uint_fast16_t x, uint_fast16_t y, uint_fast16_t width, uint_fast16_t height)
      {
        ray r;
        r.origin = vec3(0.0f, 0.0f, 0.0f);
        vec2 xy = vec2(x, y) - vec2(width, height) / 2.0f;
        xy /= vec2(width, height);
        r.direction = vec3(xy[0], xy[1], 0.5f);
        // r.direction = vec3(0.0, 0.0, 1.0f);

        r.direction = normalize(r.direction);

        r.id = uint32_t(x) + uint32_t(y) * uint32_t(width);
        r.pad = 0;
        // x = id % width;
        // y = id / width;
        return r;
      }

      std::vector<ray> init_rays(uint_fast16_t width, uint_fast16_t height)
      {
        // std::vector<ray> rays(width * height, ray());
        std::vector<ray> rays(width * height);
        for (uint_fast16_t y = 0; y < height; ++y)
        {
          for (uint_fast16_t x = 0; x < width; ++x)
          {
            uint32_t sz = width * height;
            ray r = init_ray(x, y, width, height);
            rays[r.id] = r;
          }
        }
        return rays;
      }

      void init_buffers(wgpu::Device device, uint16_t width, uint16_t height)
      {
        std::cout << "Initializing buffers..." << std::endl;
        std::cout << "  -Uniform buffer..." << std::endl;
        this->set_invocation_count(width, height);

        bindings::uniform::ptr uniforms =
            bindings::uniform::create<quat, uint32_t, uint32_t, float, float>({"orientation", "width", "height", "pad0", "pad1"}, device);
        std::cout << "  -Ray buffer..." << std::endl;

        uniforms->set_member("orientation", quat(1.0, 0.0, 0.0, 0.0));
        uniforms->set_member("width", (uint32_t)width);
        uniforms->set_member("height", (uint32_t)height);
        uniforms->set_member("pad0", 0.0f);
        uniforms->set_member("pad1", 0.0f);
        uniforms->set_visibility(wgpu::ShaderStage::Compute);

        std::vector<ray> rays = init_rays(width, height);
        bindings::buffer::ptr ray_buffer_bindings = bindings::buffer::create  ();
        ray_buffer_bindings->get_buffer()->set_label("Rays");
        ray_buffer_bindings->get_buffer()->set_usage(flags::storage::read);
        ray_buffer_bindings->set_visibility(wgpu::ShaderStage::Compute);
        ray_buffer_bindings->set_binding_type(wgpu::BufferBindingType::Storage);
        ray_buffer_bindings->get_buffer()->write<ray>(rays, device);
        std::cout << "  -Assigning buffers..." << std::endl;
        _bindings->assign(2, uniforms);
        std::cout << "  -Assigning ray buffer..." << std::endl;
        _bindings->assign(3, ray_buffer_bindings);
      }

      void init_textures(wgpu::Device device)
      {
        std::cout << "Initializing textures..." << std::endl;
        // Load image data
        lewitt::bindings::texture::ptr input_texture_binding = lewitt::bindings::texture::create(
            ResourceManager::loadTextureAndView(RESOURCE_DIR "/input.jpg", device));

        input_texture_binding->set_visibility(wgpu::ShaderStage::Compute);
        input_texture_binding->set_sample_type(wgpu::TextureSampleType::Float);
        input_texture_binding->set_dimension(wgpu::TextureViewDimension::_2D);

        std::cout << "input_texture valid: " << input_texture_binding->valid() << std::endl;

        uint32_t width = input_texture_binding->get_texture().getWidth();
        uint32_t height = input_texture_binding->get_texture().getHeight();

        std::cout << "verifying texture dimensions: " << width << "x" << height << std::endl;

        lewitt::bindings::storage_texture::ptr output_texture_binding = lewitt::bindings::storage_texture::create(
            ResourceManager::createEmptyStorageTextureAndView(width, height, device));

        output_texture_binding->set_visibility(wgpu::ShaderStage::Compute);
        output_texture_binding->set_compute_write_2d();
        std::cout << "  -Input texture..." << std::endl;
        _bindings->assign(0, input_texture_binding);
        std::cout << "  -Output texture..." << std::endl;
        _bindings->assign(1, output_texture_binding);
        init_buffers(device, width, height);
      }

      void init_shader_pipeline(wgpu::Device &device)
      {
        std::cout << "  -inititialzing layout..." << std::endl;
        _bindings->init_layout(device);
        std::cout << "  -inititialzing bind group..." << std::endl;
        _bindings->init(device);
        _shader->init(device, _bindings->get_layout());
      }

      struct ray_shader : public shaders::shader
      {
      public:
        DEFINE_CREATE_FUNC(ray_shader)
        ray_shader(wgpu::Device &device)
        {
          std::cout << "Creating shader module..." << std::endl;
          this->shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/ray-shader.wgsl", device);
        }
        ~ray_shader()
        {
        }

        bool
        init(wgpu::Device &device,
             wgpu::BindGroupLayout &bind_group_layout)
        {
          // Create compute pipeline layout
          std::cout << "init compute pipeline" << std::endl;
          wgpu::PipelineLayoutDescriptor pipelineLayoutDesc;
          pipelineLayoutDesc.bindGroupLayoutCount = 1;
          pipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout *)&bind_group_layout;
          wgpu::PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutDesc);

          // Create compute pipeline
          wgpu::ComputePipelineDescriptor computePipelineDesc;
          computePipelineDesc.compute.constantCount = 0;
          computePipelineDesc.compute.constants = nullptr;
          computePipelineDesc.compute.entryPoint = "trace";
          computePipelineDesc.compute.module = this->shaderModule;
          computePipelineDesc.layout = pipelineLayout;
          _pipeline = device.createComputePipeline(computePipelineDesc);

          return _pipeline != nullptr;
        }

        virtual wgpu::ComputePipeline compute_pipe_line()
        {
          std::cout << " getting pipeline" << std::endl;
          return _pipeline;
        }

        wgpu::ComputePipeline _pipeline = nullptr;
      };
    };

    /*
        class instanced_geometry : public renderable
        {
        public:
          GLM_TYPEDEFS;

          DEFINE_CREATE_FUNC(instanced_geometry)

          instanced_geometry(wgpu::Device device)
          {
            this->_shader = ray_shader::create(device);
            _bindings = bindings::group::create();
          }

          ~instanced_geometry() {}

          struct Uniforms
          {
            quat orientation = quat(1.0, 0.0, 0.0, 0.0);
            vec4 position;
          };

          static_assert(sizeof(ray) % 16 == 0);

          ray init_ray(uint_fast16_t x, uint_fast16_t y, uint_fast16_t width, uint_fast16_t height)
          {
            ray r;
            r.origin = vec3(0.0f, 0.0f, 0.0f);
            vec2 xy = vec2(x, y) - vec2(width, height) / 2.0f;
            xy /= vec2(width, height);
            r.direction = vec3(xy[0], xy[1], 0.5f);
            // r.direction = vec3(0.0, 0.0, 1.0f);

            r.direction = normalize(r.direction);

            r.id = uint32_t(x) + uint32_t(y) * uint32_t(width);
            r.pad = 0;
            // x = id % width;
            // y = id / width;
            return r;
          }

          std::vector<ray> init_rays(uint_fast16_t width, uint_fast16_t height)
          {
            // std::vector<ray> rays(width * height, ray());
            std::vector<ray> rays(width * height);
            for (uint_fast16_t y = 0; y < height; ++y)
            {
              for (uint_fast16_t x = 0; x < width; ++x)
              {
                uint32_t sz = width * height;
                ray r = init_ray(x, y, width, height);
                rays[r.id] = r;
              }
            }
            return rays;
          }

          void init_buffers(wgpu::Device device, uint16_t width, uint16_t height)
          {
            std::cout << "Initializing buffers..." << std::endl;
            std::cout << "  -Uniform buffer..." << std::endl;
            this->set_invocation_count(width, height);

            bindings::uniform::ptr uniforms =
                bindings::uniform::create<quat, uint32_t, uint32_t, float, float>({"orientation", "width", "height", "pad0", "pad1"}, device);
            std::cout << "  -Ray buffer..." << std::endl;

            uniforms->set_member("orientation", quat(1.0, 0.0, 0.0, 0.0));
            uniforms->set_member("width", (uint32_t)width);
            uniforms->set_member("height", (uint32_t)height);
            uniforms->set_member("pad0", 0.0f);
            uniforms->set_member("pad1", 0.0f);
            uniforms->set_visibility(wgpu::ShaderStage::Compute);

            std::vector<ray> rays = init_rays(width, height);
            bindings::buffer::ptr ray_buffer_bindings = bindings::buffer::create<ray>();
            ray_buffer_bindings->get_buffer()->set_label("Rays");
            ray_buffer_bindings->get_buffer()->set_usage(buffer_flags::storage_read);
            ray_buffer_bindings->set_visibility(wgpu::ShaderStage::Compute);
            ray_buffer_bindings->set_binding_type(wgpu::BufferBindingType::Storage);
            ray_buffer_bindings->get_buffer()->write<ray>(rays, device);
            std::cout << "  -Assigning buffers..." << std::endl;
            _bindings->assign(2, uniforms);
            std::cout << "  -Assigning ray buffer..." << std::endl;
            _bindings->assign(3, ray_buffer_bindings);
          }

          void init_textures(wgpu::Device device)
          {
            std::cout << "Initializing textures..." << std::endl;
            // Load image data
            lewitt::bindings::texture::ptr input_texture_binding = lewitt::bindings::texture::create(
                ResourceManager::loadTextureAndView(RESOURCE_DIR "/input.jpg", device));

            input_texture_binding->set_visibility(wgpu::ShaderStage::Compute);
            input_texture_binding->set_sample_type(wgpu::TextureSampleType::Float);
            input_texture_binding->set_dimension(wgpu::TextureViewDimension::_2D);

            std::cout << "input_texture valid: " << input_texture_binding->valid() << std::endl;

            uint32_t width = input_texture_binding->get_texture().getWidth();
            uint32_t height = input_texture_binding->get_texture().getHeight();

            std::cout << "verifying texture dimensions: " << width << "x" << height << std::endl;

            lewitt::bindings::storage_texture::ptr output_texture_binding = lewitt::bindings::storage_texture::create(
                ResourceManager::createEmptyStorageTextureAndView(width, height, device));

            output_texture_binding->set_visibility(wgpu::ShaderStage::Compute);
            output_texture_binding->set_compute_write_2d();
            std::cout << "  -Input texture..." << std::endl;
            _bindings->assign(0, input_texture_binding);
            std::cout << "  -Output texture..." << std::endl;
            _bindings->assign(1, output_texture_binding);
            init_buffers(device, width, height);
          }

          void init_shader_pipeline(wgpu::Device &device)
          { std::cout << "  -inititialzing layout..." << std::endl;
            _bindings->init_layout(device);
            std::cout << "  -inititialzing bind group..." << std::endl;
            _bindings->init(device);
            _shader->init(device, _bindings->get_layout());
          }

          struct ray_shader : public shaders::shader
          {
          public:
            DEFINE_CREATE_FUNC(ray_shader)
            ray_shader(wgpu::Device &device)
            {
              std::cout << "Creating shader module..." << std::endl;
              this->shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/ray-shader.wgsl", device);
            }
            ~ray_shader()
            {
            }

            bool
            init(wgpu::Device &device,
                 wgpu::BindGroupLayout &bind_group_layout)
            {
              // Create compute pipeline layout
              std::cout << "init compute pipeline" << std::endl;
              wgpu::PipelineLayoutDescriptor pipelineLayoutDesc;
              pipelineLayoutDesc.bindGroupLayoutCount = 1;
              pipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout *)&bind_group_layout;
              wgpu::PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutDesc);

              // Create compute pipeline
              wgpu::ComputePipelineDescriptor computePipelineDesc;
              computePipelineDesc.compute.constantCount = 0;
              computePipelineDesc.compute.constants = nullptr;
              computePipelineDesc.compute.entryPoint = "trace";
              computePipelineDesc.compute.module = this->shaderModule;
              computePipelineDesc.layout = pipelineLayout;
              _pipeline = device.createComputePipeline(computePipelineDesc);

              return _pipeline != nullptr;
            }

            virtual wgpu::ComputePipeline compute_pipe_line()
            {
              std::cout << " getting pipeline" << std::endl;
              return _pipeline;
            }

            wgpu::ComputePipeline _pipeline = nullptr;
          };
        };
    */
  }
}
