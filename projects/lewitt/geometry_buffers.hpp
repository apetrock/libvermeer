#pragma once
#include <stack>
#include "common.h"
#include <webgpu/webgpu.hpp>
#include "ResourceManager.h"
// there will have to be scene uniforms and buffer uniforms,
// I think we can seperate all of those out.
namespace lewitt
{
  namespace geometry
  {

    using VertexAttributes = ResourceManager::VertexAttributes;

    class material
    {
    public:
      DEFINE_CREATE_FUNC(material)
      material() {}
      ~material() {}
      wgpu::ShaderModule shaderModule = nullptr;
    };


    class foo_material : public material
    {
    public:
      DEFINE_CREATE_FUNC(foo_material)
      foo_material(wgpu::Device &device)
      {
        std::cout << "Creating shader module..." << std::endl;
        this->shaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/shader.wgsl", device);
      }
      ~foo_material() {
        m_pipeline.release();
      }

      bool initRenderPipeline(wgpu::Device &device,
                              wgpu::BindGroupLayout & bind_group_layout,
                              const wgpu::TextureFormat &color_format,
                              const wgpu::TextureFormat &depth_format)
      {

        std::cout << "Creating render pipeline..." << std::endl;
        wgpu::RenderPipelineDescriptor pipelineDesc;

        // Vertex fetch
        std::vector<wgpu::VertexAttribute> vertexAttribs(6);

        //lets attach this to the shader desc, somehow
        vertexAttribs[0].shaderLocation = 0;
        vertexAttribs[0].format = wgpu::VertexFormat::Float32x3;
        vertexAttribs[0].offset = 0;

        // Normal attribute
        vertexAttribs[1].shaderLocation = 1;
        vertexAttribs[1].format = wgpu::VertexFormat::Float32x3;
        vertexAttribs[1].offset = offsetof(VertexAttributes, normal);

        // Color attribute
        vertexAttribs[2].shaderLocation = 2;
        vertexAttribs[2].format = wgpu::VertexFormat::Float32x3;
        vertexAttribs[2].offset = offsetof(VertexAttributes, color);

        // UV attribute
        vertexAttribs[3].shaderLocation = 3;
        vertexAttribs[3].format = wgpu::VertexFormat::Float32x2;
        vertexAttribs[3].offset = offsetof(VertexAttributes, uv);

        // Tangent attribute
        vertexAttribs[4].shaderLocation = 4;
        vertexAttribs[4].format = wgpu::VertexFormat::Float32x3;
        vertexAttribs[4].offset = offsetof(VertexAttributes, tangent);

        // Bitangent attribute
        vertexAttribs[5].shaderLocation = 5;
        vertexAttribs[5].format = wgpu::VertexFormat::Float32x3;
        vertexAttribs[5].offset = offsetof(VertexAttributes, bitangent);

        wgpu::VertexBufferLayout vertexBufferLayout;
        vertexBufferLayout.attributeCount = (uint32_t)vertexAttribs.size();
        vertexBufferLayout.attributes = vertexAttribs.data();
        vertexBufferLayout.arrayStride = sizeof(VertexAttributes);
        vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;

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

        wgpu::FragmentState fragmentState;
        pipelineDesc.fragment = &fragmentState;
        fragmentState.module = this->shaderModule;
        fragmentState.entryPoint = "fs_main";
        fragmentState.constantCount = 0;
        fragmentState.constants = nullptr;

        wgpu::BlendState blendState;
        blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
        blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        blendState.color.operation = wgpu::BlendOperation::Add;
        blendState.alpha.srcFactor = wgpu::BlendFactor::Zero;
        blendState.alpha.dstFactor = wgpu::BlendFactor::One;
        blendState.alpha.operation = wgpu::BlendOperation::Add;

        wgpu::ColorTargetState colorTarget;
        colorTarget.format = color_format;
        colorTarget.blend = &blendState;
        colorTarget.writeMask = wgpu::ColorWriteMask::All;

        fragmentState.targetCount = 1;
        fragmentState.targets = &colorTarget;

        wgpu::DepthStencilState depthStencilState = wgpu::Default;
        depthStencilState.depthCompare = wgpu::CompareFunction::Less;
        depthStencilState.depthWriteEnabled = true;
        depthStencilState.format = depth_format;
        depthStencilState.stencilReadMask = 0;
        depthStencilState.stencilWriteMask = 0;

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

      wgpu::RenderPipeline & pipe_line(){return m_pipeline;}
      //wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
      // Bind Group
      //wgpu::BindGroup m_bindGroup = nullptr;
      wgpu::RenderPipeline m_pipeline = nullptr;
    };

    class buffer
    {
    public:
      DEFINE_CREATE_FUNC(buffer);
      buffer() {}

      ~buffer()
      {
        m_vertexBuffer.destroy();
        m_vertexBuffer.release();
        m_vertexCount = 0;
      }

      bool init(wgpu::Device &device, wgpu::Queue &queue)
      {
        // Load mesh data from OBJ file
        std::vector<VertexAttributes> vertexData;
        bool success = ResourceManager::loadGeometryFromObj(RESOURCE_DIR "/cylinder.obj", vertexData);
        // bool success = ResourceManager::loadGeometryFromObj(RESOURCE_DIR "/fourareen.obj", vertexData);
        if (!success)
        {
          std::cerr << "Could not load geometry!" << std::endl;
          return false;
        }

        // Create vertex buffer
        wgpu::BufferDescriptor bufferDesc;
        bufferDesc.size = vertexData.size() * sizeof(VertexAttributes);
        bufferDesc.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex;
        bufferDesc.mappedAtCreation = false;
        m_vertexBuffer = device.createBuffer(bufferDesc);
        queue.writeBuffer(m_vertexBuffer, 0, vertexData.data(), bufferDesc.size);

        m_vertexCount = static_cast<int>(vertexData.size());

        return m_vertexBuffer != nullptr;
      }


      void set_current(wgpu::RenderPassEncoder render_pass)
      {
        render_pass.setVertexBuffer(0, m_vertexBuffer, 0, m_vertexBuffer.getSize());
      }
      
      wgpu::Buffer & get_buffer(){
        return m_vertexBuffer;
      }
      int count() {return m_vertexCount;}
      int size(){return m_vertexCount * sizeof(VertexAttributes);}

      wgpu::Buffer m_vertexBuffer = nullptr;
      int m_vertexCount = 0;
      // material::ptr _material;
    };

  }
}