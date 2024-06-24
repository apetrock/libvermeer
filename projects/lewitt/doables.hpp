#pragma once
#include <stack>
#include "common.h"
#include <webgpu/webgpu.hpp>
#include "ResourceManager.h"

#include "bindings.hpp"
#include "vertex_formats.hpp"
#include "shaders.hpp"
#include "buffers.hpp"
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
          const buffers::buffer::ptr &buffer,
          const bindings::group::ptr &bindings,
          const shaders::shader::ptr &shader)
      {
        ptr obj = std::make_shared<renderable>();
        obj->set_bindings(bindings);
        obj->set_buffer(buffer);
        obj->set_shader(shader);
        return obj;
      }

      renderable()
      {
      }
      ~renderable() {}

      void draw(wgpu::RenderPassEncoder renderpass)
      {

        renderpass.setPipeline(_shader->pipe_line());
        /// renderPass.setVertexBuffer(0, m_vertexBuffer, 0, m_vertexCount * sizeof(VertexAttributes));
        _buffer->set_current(renderpass);
        renderpass.setBindGroup(0, _bindings->get_group(), 0, nullptr);
        renderpass.draw(_buffer->count(), 1, 0, 0);
      }

      void set_buffer(const buffers::buffer::ptr &buffer)
      {
        _buffer = buffer;
      }

      buffers::buffer::ptr _buffer;
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

      void draw(wgpu::RenderPassEncoder renderpass)
      {
      }
    };

    class ray_compute : public computable
    {
      GLM_TYPEDEFS;
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

      using ptr = std::shared_ptr<ray_compute>;
      ray_compute() {}
      ~ray_compute() {}

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
        bindings::uniform::ptr uniforms =
            bindings::uniform::create<quat, uint32_t, uint32_t, float, float>({"orientation", "width", "height", "pad0", "pad1"}, device);
        std::cout
            << "  -Ray buffer..." << std::endl;

        std::vector<ray> rays = init_rays(width, height);
        bindings::buffer::ptr ray_buffer_bindings = bindings::buffer::create<ray>();
        ray_buffer_bindings->get_buffer()->set_label("Rays");
        ray_buffer_bindings->get_buffer()->set_usage(buffers::buffer_read);
        ray_buffer_bindings->get_buffer()->write<ray>(rays, device);
        _bindings->insert(2, uniforms);
        _bindings->insert(3, ray_buffer_bindings);
      }

      void init_textures(wgpu::Device device)
      {
        std::cout << "Initializing textures..." << std::endl;
        // Load image data
        lewitt::bindings::texture::ptr input_texture_binding = lewitt::bindings::texture::create(
            ResourceManager::loadTextureAndView(RESOURCE_DIR "/input.jpg", device));

        uint32_t width = input_texture_binding->get_texture().getWidth();
        uint32_t height = input_texture_binding->get_texture().getHeight();

        wgpu::TextureDescriptor textureDesc;
        textureDesc.dimension = wgpu::TextureDimension::_2D;
        textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
        textureDesc.size = {width, height, 1};
        textureDesc.sampleCount = 1;
        textureDesc.viewFormatCount = 0;
        textureDesc.viewFormats = nullptr;
        textureDesc.mipLevelCount = 1;
        textureDesc.label = "Output";
        textureDesc.usage = buffers::buffer_write;

        TextureViewDescriptor textureViewDesc;
        textureViewDesc.aspect = TextureAspect::All;
        textureViewDesc.baseArrayLayer = 0;
        textureViewDesc.arrayLayerCount = 1;
        textureViewDesc.dimension = TextureViewDimension::_2D;
        textureViewDesc.format = TextureFormat::RGBA8Unorm;
        textureViewDesc.mipLevelCount = 1;
        textureViewDesc.baseMipLevel = 0;

        wgpu::Texture output_tex = device.createTexture(textureDesc);
        textureViewDesc.label = "Output";
        wgpu::TextureView output_tex_view = output_tex.createView(textureViewDesc);
        lewitt::bindings::texture::ptr output_texture_binding = lewitt::bindings::storage_texture::create(
            output_tex, output_tex_view);
        output_texture_binding->set_visibility(wgpu::ShaderStage::Compute);
        _bindings->insert(0, input_texture_binding);
        _bindings->insert(1, output_texture_binding);
      }
    };
  }

}
