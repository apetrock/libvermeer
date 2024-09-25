#pragma once
#include <stack>
#include "common.h"
#include <webgpu/webgpu.hpp>
#include "resources.hpp"
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
        if (shaderModule)
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
                        wgpu::TextureFormat depth_format,
                        std::string vertex_entry = "vs_main",
                        std::string fragment_entry = "fs_main")
      {
        std::cout << "base render init" << std::endl;

        return false;
      }

      virtual void add_layout(
          wgpu::VertexBufferLayout layout)
      {
        _layouts.push_back(layout);
      }
      virtual wgpu::RenderPipeline render_pipe_line() { return nullptr; }
      virtual wgpu::ComputePipeline compute_pipe_line() { return nullptr; }

      std::vector<wgpu::VertexBufferLayout> _layouts;
      std::vector<vertex_formats::Format> _formats;
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
        wgpu::ShaderModule shader_module, int buffer_count = 1)
    {
      wgpu::VertexState state;
      state.bufferCount = buffer_count;
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
        this->shaderModule = resources::load_shader_module(RESOURCE_DIR "/pnc.wgsl", device);
      }
      ~PN()
      {
        if (m_pipeline)
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
        std::vector<wgpu::VertexAttribute> vertex_format_t = vertex_formats::PN_attrib();
        wgpu::VertexBufferLayout vertexBufferLayout_t = vertex_formats::create_vertex_layout<vertex_formats::PN_t>(vertex_format_t);
        auto [vertex_format, vertexBufferLayout] = vertex_formats::create_PN_vertex_layout(wgpu::VertexStepMode::Vertex);
        std::cout << vertex_format.size() << " " << vertex_format_t.size() << std::endl;
        for (int i = 0; i < vertex_format.size(); i++)
        {
          std::cout << vertex_format[i].shaderLocation << " " << vertex_format_t[i].shaderLocation << std::endl;
          std::cout << vertex_format[i].format << " " << vertex_format_t[i].format << std::endl;
          std::cout << vertex_format[i].offset << " " << vertex_format_t[i].offset << std::endl;
        }
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
        this->shaderModule = resources::load_shader_module(RESOURCE_DIR "/pncuvtb.wgsl", device);
      }
      ~PNCUVTB()
      {
        if (m_pipeline)
          m_pipeline.release();
      }

      virtual bool
      init(wgpu::Device device,
           wgpu::BindGroupLayout bind_group_layout,
           wgpu::TextureFormat color_format,
           wgpu::TextureFormat depth_format,
           std::string vertex_entry = "vs_main",
           std::string fragment_entry = "fs_main")
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

    class render_shader : public shader
    {
    public:

      using ptr = std::shared_ptr<render_shader>;

      static ptr create_from_path(std::string path, wgpu::Device &device){
        ptr shader = std::make_shared<render_shader>();
        shader->shaderModule = resources::load_shader_module(path, device);
        return shader;
      }
      
      static ptr create_from_src(std::string src, wgpu::Device &device){
        ptr shader = std::make_shared<render_shader>();
        shader->shaderModule = resources::create_shader_module(src, device);
        return shader;
      }

      render_shader()
      {
      }

      ~render_shader()
      {
        if (m_pipeline)
          m_pipeline.release();
      }

      virtual bool
      init(wgpu::Device device,
           wgpu::BindGroupLayout bind_group_layout,
           wgpu::TextureFormat color_format,
           wgpu::TextureFormat depth_format,
           std::string vertex_entry = "vs_main",
           std::string fragment_entry = "fs_main")
      {

        std::cout << "Creating render pipeline..." << std::endl;
        wgpu::RenderPipelineDescriptor pipelineDesc;

        pipelineDesc.vertex = basic_vertex_state(vertex_entry.c_str(), this->shaderModule, _layouts.size());
        pipelineDesc.vertex.buffers = _layouts.data();
        std::cout << "layouts: " << _layouts.size() << std::endl;
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

      virtual wgpu::RenderPipeline pipeline() { return m_pipeline; }
      // wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
      //  Bind Group
      // wgpu::BindGroup m_bindGroup = nullptr;
      bindings::group::ptr _bind_group;
      wgpu::RenderPipeline m_pipeline = nullptr;
    };


  class compute_shader : public shaders::shader { 
    public:
    using ptr = std::shared_ptr<compute_shader>;
    
    static ptr create_from_path(const std::string & path, wgpu::Device &device){
      ptr shader = std::make_shared<compute_shader>();
      shader->shaderModule = resources::load_shader_module(path, device);
      return shader;
    }

    static ptr create_from_src(const std::string & src, wgpu::Device &device){
      ptr shader = std::make_shared<compute_shader>();
      shader->shaderModule = resources::create_shader_module(src, device);
      return shader;
    }
    
    compute_shader(){
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

    virtual wgpu::ComputePipeline pipeline()
    {
      return _pipeline;
    }

    wgpu::ComputePipeline _pipeline = nullptr;

  };

  }
}