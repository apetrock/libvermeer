#pragma once
#include <numeric>
#include <algorithm>
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

    GLM_TYPEDEFS;
    // these probably aren't needed
    struct vec3_t
    {
      vec3 v;
    };

    GLM_TYPEDEFS;
    struct vec4_t
    {
      vec4 v;
    };

    using VertexAttributes = ResourceManager::VertexAttributes;
    using PNCUVTB_t = ResourceManager::VertexAttributes;
    inline std::vector<wgpu::VertexAttribute> PNCUVTB_attrib()
    {
      // Vertex fetch
      const int N_attribs = 6;
      std::vector<wgpu::VertexAttribute> vertexAttribs(N_attribs);
      int offsets[N_attribs] = {
          0, offsetof(PNCUVTB_t, normal),
          offsetof(PNCUVTB_t, color), offsetof(PNCUVTB_t, uv),
          offsetof(PNCUVTB_t, tangent), offsetof(PNCUVTB_t, bitangent)};

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

    using PN_t = ResourceManager::position_normal_attributes;
    inline std::vector<wgpu::VertexAttribute> PN_attrib()
    {
      // Vertex fetch
      const int N_attribs = 2;
      std::vector<wgpu::VertexAttribute> vertexAttribs(N_attribs);
      int offsets[N_attribs] = {0, offsetof(PN_t, normal)};

      wgpu::VertexFormat formats[N_attribs] = {
          wgpu::VertexFormat::Float32x3, wgpu::VertexFormat::Float32x3};

      for (int i = 0; i < N_attribs; i++)
      {
        vertexAttribs[i].shaderLocation = i;
        vertexAttribs[i].format = formats[i];
        vertexAttribs[i].offset = offsets[i];
      }
      return vertexAttribs;
    }

    inline std::vector<wgpu::VertexAttribute> vec3_attrib()
    {
      // Vertex fetch
      const int N_attribs = 1;
      std::vector<wgpu::VertexAttribute> vertexAttribs(N_attribs);
      int offsets[N_attribs] = {0};

      wgpu::VertexFormat formats[N_attribs] = {
          wgpu::VertexFormat::Float32x3};

      for (int i = 0; i < N_attribs; i++)
      {
        vertexAttribs[i].shaderLocation = i;
        vertexAttribs[i].format = formats[i];
        vertexAttribs[i].offset = offsets[i];
      }
      return vertexAttribs;
    }

    inline std::vector<wgpu::VertexAttribute> vec4_attrib()
    {
      // Vertex fetch
      const int N_attribs = 1;
      std::vector<wgpu::VertexAttribute> vertexAttribs(N_attribs);
      int offsets[N_attribs] = {0};

      wgpu::VertexFormat formats[N_attribs] = {
          wgpu::VertexFormat::Float32x4};

      for (int i = 0; i < N_attribs; i++)
      {
        vertexAttribs[i].shaderLocation = i;
        vertexAttribs[i].format = formats[i];
        vertexAttribs[i].offset = offsets[i];
      }
      return vertexAttribs;
    }

    // just the floats for now
    template <typename T>
    inline wgpu::VertexFormat type_alias()
    {
      if constexpr (std::is_same_v<T, float>)
        return wgpu::VertexFormat::Float32;
      else if constexpr (std::is_same_v<T, vec2>)
        return wgpu::VertexFormat::Float32x2;
      else if constexpr (std::is_same_v<T, vec3>)
        return wgpu::VertexFormat::Float32x3;
      else if constexpr (std::is_same_v<T, vec4>)
        return wgpu::VertexFormat::Float32x4;
      else if constexpr (std::is_same_v<T, quat>)
        return wgpu::VertexFormat::Float32x4;
    }

    template <typename T>
    wgpu::VertexBufferLayout create_vertex_layout(std::vector<wgpu::VertexAttribute> &format,
                                                  wgpu::VertexStepMode step_mode = wgpu::VertexStepMode::Vertex)
    {
      wgpu::VertexBufferLayout vertexBufferLayout;
      vertexBufferLayout.attributeCount = (uint32_t)format.size();
      vertexBufferLayout.attributes = format.data();
      vertexBufferLayout.arrayStride = sizeof(T);
      vertexBufferLayout.stepMode = step_mode;

      return vertexBufferLayout;
    }

    template <typename... Types>
    inline std::vector<wgpu::VertexAttribute> attribute()
    {

      size_t N = sizeof...(Types);
      std::vector<wgpu::VertexAttribute> vertexAttribs(N);

      std::vector<size_t> sizes = {sizeof(Types)...};
      std::vector<wgpu::VertexFormat> formats = {type_alias<Types>()...};
      std::size_t offset = 0;

      for (int i = 0; i < N; i++)
      {
        vertexAttribs[i].shaderLocation = i;
        vertexAttribs[i].format = formats[i];
        vertexAttribs[i].offset = offset;
        offset += sizes[i];
      }
      return vertexAttribs;
    }

    template <typename... Types>
    std::tuple<std::vector<wgpu::VertexAttribute>, wgpu::VertexBufferLayout>
    create_vertex_layout(wgpu::VertexStepMode step_mode = wgpu::VertexStepMode::Vertex)
    {
      std::vector<size_t> sizes = {sizeof(Types)...};
      size_t totalSize = std::reduce(sizes.begin(), sizes.end(), 0);
      std::vector<wgpu::VertexAttribute> format = attribute<Types...>();
      
      wgpu::VertexBufferLayout vertexBufferLayout;
      vertexBufferLayout.attributeCount = (uint32_t)format.size();
      vertexBufferLayout.attributes = format.data();
      vertexBufferLayout.arrayStride = totalSize;
      vertexBufferLayout.stepMode = step_mode;

      return {format, vertexBufferLayout};
    }

  }
}