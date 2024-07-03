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
        return false;
      }

      virtual bool init(wgpu::Device &device,
                        wgpu::BindGroupLayout &bind_group_layout,
                        const wgpu::TextureFormat &color_format,
                        const wgpu::TextureFormat &depth_format)
      {
        return false;
      }
      virtual wgpu::RenderPipeline render_pipe_line() { return nullptr; }
      virtual wgpu::ComputePipeline compute_pipe_line() { return nullptr; }
    };

    inline wgpu::BlendState standard_blend_state()
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

    inline wgpu::ColorTargetState standard_color_target(wgpu::TextureFormat color_format,
                                                 const wgpu::BlendState & blendState)
    {
      wgpu::ColorTargetState colorTarget;
      colorTarget.format = color_format;
      colorTarget.blend = &blendState;
      colorTarget.writeMask = wgpu::ColorWriteMask::All;
      return colorTarget;
    }

    inline wgpu::DepthStencilState standard_depth_stencil_state(wgpu::TextureFormat depth_format)
    {

      wgpu::DepthStencilState depthStencilState = wgpu::Default;
      depthStencilState.depthCompare = wgpu::CompareFunction::Less;
      depthStencilState.depthWriteEnabled = true;
      depthStencilState.format = depth_format;
      depthStencilState.stencilReadMask = 0;
      depthStencilState.stencilWriteMask = 0;
      return depthStencilState;
    }

    inline wgpu::RenderPipeline standard_triangle_topology_pipeline(
        std::string vertex_shader_entry_point,
        std::string fragment_shader_entry_point,
        wgpu::Device &device,
        wgpu::BindGroupLayout &bind_group_layout,
        const wgpu::TextureFormat &color_format,
        const wgpu::TextureFormat &depth_format)
    {
      return nullptr;
    }

    class NCUVTB : public shader
    {
    public:
      DEFINE_CREATE_FUNC(NCUVTB)
      NCUVTB(wgpu::Device &device)
      {
        std::cout << "Creating shader module..." << std::endl;
        this->shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
      }
      ~NCUVTB()
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
        std::vector<wgpu::VertexAttribute> vertexAttribs = vertex_formats::NCUVTB();
        wgpu::VertexBufferLayout vertexBufferLayout = vertex_formats::create_layout<vertex_formats::NCUVTB_t>(vertexAttribs);

        pipelineDesc.vertex.bufferCount = 1;
        pipelineDesc.vertex.buffers = &vertexBufferLayout;

        pipelineDesc.vertex.module = this->shaderModule;
        pipelineDesc.vertex.entryPoint = "vs_main";
        pipelineDesc.vertex.constantCount = 0;
        pipelineDesc.vertex.constants = nullptr;

        pipelineDesc.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        pipelineDesc.primitive.stripIndexFormat = wgpu::IndexFormat::Undefined;
        pipelineDesc.primitive.frontFace = wgpu::FrontFace::CCW;
        pipelineDesc.primitive.cullMode = wgpu::CullMode::None;

        wgpu::BlendState blendState = standard_blend_state();
        wgpu::ColorTargetState colorTarget = standard_color_target(color_format, blendState);
        wgpu::DepthStencilState depthStencilState = standard_depth_stencil_state(depth_format);

        wgpu::FragmentState fragmentState;
        pipelineDesc.fragment = &fragmentState;
        fragmentState.module = this->shaderModule;
        fragmentState.entryPoint = "fs_main";
        fragmentState.constantCount = 0;
        fragmentState.constants = nullptr;
        fragmentState.targetCount = 1;
        fragmentState.targets = &colorTarget;

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