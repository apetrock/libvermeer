#pragma once
#include <stack>
#include "common.h"
#include <webgpu/webgpu.hpp>
#include "ResourceManager.h"
#include "bindings.hpp"
// there will have to be scene uniforms and buffer uniforms,
// I think we can seperate all of those out.
namespace lewitt
{
  namespace vertex_formats
  {

    using VertexAttributes = ResourceManager::VertexAttributes;
    using NCUVTB_t = ResourceManager::VertexAttributes;
    inline std::vector<wgpu::VertexAttribute> NCUVTB()
    {
      // Vertex fetch
      const int N_attribs = 6;
      std::vector<wgpu::VertexAttribute> vertexAttribs(N_attribs);
      int offsets[N_attribs] = {
          0, offsetof(NCUVTB_t, normal),
          offsetof(NCUVTB_t, color), offsetof(NCUVTB_t, uv),
          offsetof(NCUVTB_t, tangent), offsetof(NCUVTB_t, bitangent)};

      wgpu::VertexFormat formats[N_attribs] = {
          wgpu::VertexFormat::Float32x3, wgpu::VertexFormat::Float32x3,
          wgpu::VertexFormat::Float32x3, wgpu::VertexFormat::Float32x3,
          wgpu::VertexFormat::Float32x3, wgpu::VertexFormat::Float32x3};

      for (int i = 0; i < N_attribs; i++)
      {
        vertexAttribs[i].shaderLocation = i;
        vertexAttribs[i].format = formats[i];
        vertexAttribs[i].offset = offsets[i];
      }
      return vertexAttribs;
    }

    template <typename T>
    wgpu::VertexBufferLayout layout(std::vector<wgpu::VertexAttribute> &format)
    {
      wgpu::VertexBufferLayout vertexBufferLayout;
      vertexBufferLayout.attributeCount = (uint32_t)format.size();
      vertexBufferLayout.attributes = format.data();
      vertexBufferLayout.arrayStride = sizeof(T);
      vertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
      return vertexBufferLayout;
    }
  }
}