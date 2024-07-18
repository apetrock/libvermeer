#pragma once
#include <stack>
#include "common.h"
#include <webgpu/webgpu.hpp>
#include "ResourceManager.h"
#include "bindings.hpp"
#include "vertex_formats.hpp"
// there will have to be scene uniforms and buffer uniforms,
// I think we can seperate all of those out.
namespace lewitt
{
  namespace shaders
  {

    class shader
    {
    public:
      using ptr = std::shared_ptr<shader>;
      shader() {}
      ~shader()
      {
        shaderModule.release();
      }

      wgpu::ShaderModule shaderModule = nullptr;
      virtual bool init(wgpu::Device &device,
                        wgpu::BindGroupLayout &bind_group_layout)
      {
        std::cout << "base compute init" << std::endl;
        return false;
      }

      virtual bool init(wgpu::Device device,
                        wgpu::BindGroupLayout bind_group_layout,
                        wgpu::TextureFormat color_format,
                        wgpu::TextureFormat depth_format)
      {
        std::cout << "base render init" << std::endl;

        return false;
      }
      virtual wgpu::RenderPipeline render_pipe_line() { return nullptr; }
      virtual wgpu::ComputePipeline compute_pipe_line() { return nullptr; }
    };

    inline wgpu::BlendState basic_blend_state()
    {
      wgpu::BlendState blendState;
      blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
      blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
      blendState.color.operation = wgpu::BlendOperation::Add;
      blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
      blendState.alpha.dstFactor = wgpu::BlendFactor::One;
      blendState.alpha.operation = wgpu::BlendOperation::Add;
      return blendState;
    }

    inline wgpu::ColorTargetState basic_color_target(wgpu::TextureFormat color_format,
                                                     const wgpu::BlendState &blendState)
    {
      wgpu::ColorTargetState colorTarget;
      colorTarget.format = color_format;
      colorTarget.blend = &blendState;
      colorTarget.writeMask = wgpu::ColorWriteMask::All;
      return colorTarget;
    }

    inline wgpu::DepthStencilState basic_depth_stencil_state(wgpu::TextureFormat depth_format)
    {

      wgpu::DepthStencilState depthStencilState = wgpu::Default;
      depthStencilState.depthCompare = wgpu::CompareFunction::Less;
      depthStencilState.depthWriteEnabled = true;
      depthStencilState.format = depth_format;
      depthStencilState.stencilReadMask = 0;
      depthStencilState.stencilWriteMask = 0;
      return depthStencilState;
    }

    inline wgpu::VertexState basic_vertex_state(
        const char *entry_point,
        wgpu::ShaderModule shader_module)
    {
      wgpu::VertexState state;
      state.bufferCount = 1;
      state.module = shader_module;
      state.entryPoint = entry_point;
      state.constantCount = 0;
      state.constants = nullptr;
      return state;
    }

    inline wgpu::FragmentState basic_fragment_state(
        const char *entry_point,
        wgpu::ShaderModule shader_module)
    {
      wgpu::FragmentState state;
      state.module = shader_module;
      state.entryPoint = entry_point;
      state.constantCount = 0;
      state.constants = nullptr;
      state.targetCount = 1;
      return state;
    }

    inline wgpu::PrimitiveState basic_primitive_state(
        wgpu::PrimitiveTopology topology,
        wgpu::IndexFormat index_format)
    {
      wgpu::PrimitiveState state;
      state.topology = topology;
      state.stripIndexFormat = index_format;
      state.frontFace = wgpu::FrontFace::CCW;
      state.cullMode = wgpu::CullMode::None;
      return state;
    }

    class PN : public shader
    {
    public:
      DEFINE_CREATE_FUNC(PN)
      PN(wgpu::Device &device)
      {
        std::cout << "Creating shader module..." << std::endl;
        this->shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/pnc.wgsl", device);
      }
      ~PN()
      {
        m_pipeline.release();
      }

      bool
      init(wgpu::Device device,
           wgpu::BindGroupLayout bind_group_layout,
           wgpu::TextureFormat color_format,
           wgpu::TextureFormat depth_format)
      {

        std::cout << "Creating render pipeline..." << std::endl;
        wgpu::RenderPipelineDescriptor pipelineDesc;
        std::vector<wgpu::VertexAttribute> vertex_format = vertex_formats::PN_attrib();
        wgpu::VertexBufferLayout vertexBufferLayout = vertex_formats::create_vertex_layout<vertex_formats::PN_t>(vertex_format);

        pipelineDesc.vertex = basic_vertex_state("vs_main", this->shaderModule);
        pipelineDesc.vertex.buffers = &vertexBufferLayout;

        pipelineDesc.primitive = basic_primitive_state(wgpu::PrimitiveTopology::TriangleList,
                                                       wgpu::IndexFormat::Undefined);

        wgpu::BlendState blendState = basic_blend_state();
        wgpu::ColorTargetState colorTarget = basic_color_target(color_format, blendState);
        wgpu::DepthStencilState depthStencilState = basic_depth_stencil_state(depth_format);

        wgpu::FragmentState fragmentState = basic_fragment_state("fs_main", this->shaderModule);
        fragmentState.targets = &colorTarget;

        pipelineDesc.fragment = &fragmentState;
        pipelineDesc.depthStencil = &depthStencilState;

        pipelineDesc.multisample.count = 1;
        pipelineDesc.multisample.mask = ~0u;
        pipelineDesc.multisample.alphaToCoverageEnabled = false;

        // Create the pipeline layout
        wgpu::PipelineLayoutDescriptor layoutDesc{};
        layoutDesc.bindGroupLayoutCount = 1;
        layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout *)&bind_group_layout;
        wgpu::PipelineLayout layout = device.createPipelineLayout(layoutDesc);
        pipelineDesc.layout = layout;

        m_pipeline = device.createRenderPipeline(pipelineDesc);
        std::cout << "Render pipeline: " << m_pipeline << std::endl;

        return m_pipeline != nullptr;
      }

      virtual wgpu::RenderPipeline render_pipe_line() { return m_pipeline; }
      // wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
      //  Bind Group
      // wgpu::BindGroup m_bindGroup = nullptr;
      bindings::group::ptr _bind_group;
      wgpu::RenderPipeline m_pipeline = nullptr;
    };

    class PNCUVTB : public shader
    {
    public:
      DEFINE_CREATE_FUNC(PNCUVTB)
      PNCUVTB(wgpu::Device &device)
      {
        std::cout << "Creating shader module..." << std::endl;
        this->shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/pncuvtb.wgsl", device);
      }
      ~PNCUVTB()
      {
        m_pipeline.release();
      }

      virtual bool
      init(wgpu::Device device,
           wgpu::BindGroupLayout bind_group_layout,
           wgpu::TextureFormat color_format,
           wgpu::TextureFormat depth_format)
      {

        std::cout << "Creating render pipeline..." << std::endl;
        wgpu::RenderPipelineDescriptor pipelineDesc;
        std::vector<wgpu::VertexAttribute> vertex_format = vertex_formats::PNCUVTB_attrib();
        wgpu::VertexBufferLayout vertexBufferLayout = vertex_formats::create_vertex_layout<vertex_formats::PNCUVTB_t>(vertex_format);

        pipelineDesc.vertex = basic_vertex_state("vs_main", this->shaderModule);
        pipelineDesc.vertex.buffers = &vertexBufferLayout;

        pipelineDesc.primitive = basic_primitive_state(wgpu::PrimitiveTopology::TriangleList,
                                                       wgpu::IndexFormat::Undefined);

        wgpu::BlendState blendState = basic_blend_state();
        wgpu::ColorTargetState colorTarget = basic_color_target(color_format, blendState);
        wgpu::DepthStencilState depthStencilState = basic_depth_stencil_state(depth_format);

        wgpu::FragmentState fragmentState = basic_fragment_state("fs_main", this->shaderModule);
        fragmentState.targets = &colorTarget;

        pipelineDesc.fragment = &fragmentState;
        pipelineDesc.depthStencil = &depthStencilState;

        pipelineDesc.multisample.count = 1;
        pipelineDesc.multisample.mask = ~0u;
        pipelineDesc.multisample.alphaToCoverageEnabled = false;

        // Create the pipeline layout
        wgpu::PipelineLayoutDescriptor layoutDesc{};
        layoutDesc.bindGroupLayoutCount = 1;
        layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout *)&bind_group_layout;
        wgpu::PipelineLayout layout = device.createPipelineLayout(layoutDesc);
        pipelineDesc.layout = layout;

        m_pipeline = device.createRenderPipeline(pipelineDesc);
        std::cout << "Render pipeline: " << m_pipeline << std::endl;

        return m_pipeline != nullptr;
      }

      virtual wgpu::RenderPipeline render_pipe_line() { return m_pipeline; }
      // wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
      //  Bind Group
      // wgpu::BindGroup m_bindGroup = nullptr;
      bindings::group::ptr _bind_group;
      wgpu::RenderPipeline m_pipeline = nullptr;
    };

    template <typename... Types>
    class shader_t : public shader
    {
    public:
      DEFINE_CREATE_FUNC(shader_t)
      shader_t(std::string path, wgpu::Device &device)
      {
        std::cout << "Creating shader module..." << std::endl;
        this->shaderModule = ResourceManager::loadShaderModule(path, device);
      }
      ~shader_t()
      {
        m_pipeline.release();
      }

      bool
      init(wgpu::Device device,
           wgpu::BindGroupLayout bind_group_layout,
           wgpu::TextureFormat color_format,
           wgpu::TextureFormat depth_format,
           std::string vertex_entry = "vs_main",
           std::string fragment_entry = "fs_main")
      {

        std::cout << "Creating render pipeline..." << std::endl;
        wgpu::RenderPipelineDescriptor pipelineDesc;
        
        auto [vertex_format, vertexBufferLayout] = vertex_formats::create_vertex_layout<Types...>();
        for (int i = 0; i < vertex_format.size(); i++)
        {
          std::cout << "format: " << i << " "<< vertex_format[i].shaderLocation << " " << vertex_format[i].format << " " << vertex_format[i].offset << std::endl;
        }

        pipelineDesc.vertex = basic_vertex_state(vertex_entry.c_str(), this->shaderModule);
        pipelineDesc.vertex.buffers = &vertexBufferLayout;

        pipelineDesc.primitive = basic_primitive_state(wgpu::PrimitiveTopology::TriangleList,
                                                       wgpu::IndexFormat::Undefined);

        wgpu::BlendState blendState = basic_blend_state();
        wgpu::ColorTargetState colorTarget = basic_color_target(color_format, blendState);
        wgpu::DepthStencilState depthStencilState = basic_depth_stencil_state(depth_format);

        wgpu::FragmentState fragmentState = basic_fragment_state(fragment_entry.c_str(), this->shaderModule);
        fragmentState.targets = &colorTarget;

        pipelineDesc.fragment = &fragmentState;
        pipelineDesc.depthStencil = &depthStencilState;

        pipelineDesc.multisample.count = 1;
        pipelineDesc.multisample.mask = ~0u;
        pipelineDesc.multisample.alphaToCoverageEnabled = false;

        // Create the pipeline layout
        wgpu::PipelineLayoutDescriptor layoutDesc{};
        layoutDesc.bindGroupLayoutCount = 1;
        layoutDesc.bindGroupLayouts = (WGPUBindGroupLayout *)&bind_group_layout;
        wgpu::PipelineLayout layout = device.createPipelineLayout(layoutDesc);
        pipelineDesc.layout = layout;

        m_pipeline = device.createRenderPipeline(pipelineDesc);
        std::cout << "Render pipeline: " << m_pipeline << std::endl;

        return m_pipeline != nullptr;
      }

      virtual wgpu::RenderPipeline render_pipe_line() { return m_pipeline; }
      // wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
      //  Bind Group
      // wgpu::BindGroup m_bindGroup = nullptr;
      bindings::group::ptr _bind_group;
      wgpu::RenderPipeline m_pipeline = nullptr;
    };

  }
}