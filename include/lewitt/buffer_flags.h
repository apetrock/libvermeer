#ifndef BUFFER_FLAGS_H
#define BUFFER_FLAGS_H

#include <webgpu/webgpu.hpp>

namespace lewitt
{
    namespace flags
    {
        namespace vertex
        {
            inline constexpr WGPUBufferUsageFlags read =
                (wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex);
        }
        namespace storage
        {
            inline constexpr WGPUBufferUsageFlags read =
                (wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage);
            inline constexpr WGPUBufferUsageFlags read_copy =
                (wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage);
            inline constexpr WGPUBufferUsageFlags copy =
                (wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::Storage);
            inline constexpr WGPUBufferUsageFlags write =
                (wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopySrc);
            inline constexpr WGPUBufferUsageFlags map =
                (wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::MapRead);
        
        }   
        namespace uniform
        {
            inline constexpr WGPUBufferUsageFlags read =
                (wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform);

        }
        namespace index
        {
            inline constexpr WGPUBufferUsageFlags read =
                (wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index);
            inline constexpr WGPUBufferUsageFlags vertex_read =
                (wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index | wgpu::BufferUsage::Vertex);
        }
        namespace texture
        {
            inline constexpr WGPUBufferUsageFlags read =
                (wgpu::TextureUsage::TextureBinding | // to bind the texture in a shader
                 wgpu::TextureUsage::CopyDst);        // to upload the input data

            inline constexpr WGPUBufferUsageFlags write =
                (wgpu::TextureUsage::TextureBinding | // to bind the texture in a shader
                 wgpu::TextureUsage::StorageBinding | // to write the texture in a shader
                 wgpu::TextureUsage::CopySrc);
        }

    }
}

#endif // BUFFER_FLAGS_H