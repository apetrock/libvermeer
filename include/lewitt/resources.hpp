/**
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://github.com/eliemichel/LearnWebGPU
 *
 * MIT License
 * Copyright (c) 2022-2023 Elie Michel
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <webgpu/webgpu.hpp>
#include <glm/glm.hpp>

#include <vector>
#include <filesystem>
#include "tiny_obj_loader.h"
#include "common.h"

#include "buffer_flags.h"

#include "stb_image.h"

#include <fstream>
#include <cstring>
namespace lewitt
{
	namespace resources
	{
		using path = std::filesystem::path;
		using vec3 = glm::vec3;
		using vec2 = glm::vec2;
		using mat3x3 = glm::mat3x3;

		struct VertexAttributes
		{
			vec3 position;

			// Texture mapping attributes represent the local frame in which
			// normals sampled from the normal map are expressed.
			vec3 tangent;		// T = local X axis
			vec3 bitangent; // B = local Y axis
			vec3 normal;		// N = local Z axis

			vec3 color;
			vec2 uv;
		};

		struct position_normal_attributes
		{
			vec3 position;
			vec3 normal; // N = local Z axis
		};

		// Equivalent of std::bit_width that is available from C++20 onward
		inline uint32_t bit_width(uint32_t m)
		{
			if (m == 0)
				return 0;
			else
			{
				uint32_t w = 0;
				while (m >>= 1)
					++w;
				return w;
			}
		}

		inline wgpu::ShaderModule create_shader_module(const std::string & src, wgpu::Device device){

			wgpu::ShaderModuleWGSLDescriptor shaderCodeDesc;
			shaderCodeDesc.chain.next = nullptr;
			shaderCodeDesc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
			shaderCodeDesc.code = src.c_str();
			wgpu::ShaderModuleDescriptor shaderDesc;
			shaderDesc.nextInChain = &shaderCodeDesc.chain;
#ifdef WEBGPU_BACKEND_WGPU
			shaderDesc.hintCount = 0;
			shaderDesc.hints = nullptr;
#endif

			return device.createShaderModule(shaderDesc);

		}
		// Load a shader from a WGSL file into a new shader module
		inline wgpu::ShaderModule load_shader_module(const path &path, wgpu::Device device)
		{
			std::ifstream file(path);
			if (!file.is_open())
			{
				return nullptr;
			}
			file.seekg(0, std::ios::end);
			size_t size = file.tellg();
			std::string shaderSource(size, ' ');
			file.seekg(0);
			file.read(shaderSource.data(), size);
			return create_shader_module(shaderSource, device);
		};

				inline mat3x3 computeTBN(const VertexAttributes corners[3], const vec3 &expectedN)
		{
			// What we call e in the figure
			vec3 ePos1 = corners[1].position - corners[0].position;
			vec3 ePos2 = corners[2].position - corners[0].position;

			// What we call \bar e in the figure
			vec2 eUV1 = corners[1].uv - corners[0].uv;
			vec2 eUV2 = corners[2].uv - corners[0].uv;

			vec3 T = normalize(ePos1 * eUV2.y - ePos2 * eUV1.y);
			vec3 B = normalize(ePos2 * eUV1.x - ePos1 * eUV2.x);
			vec3 N = cross(T, B);

			// Fix overall orientation
			if (dot(N, expectedN) < 0.0)
			{
				T = -T;
				B = -B;
				N = -N;
			}

			// Ortho-normalize the (T, B, expectedN) frame
			// a. "Remove" the part of T that is along expected N
			N = expectedN;
			T = normalize(T - dot(T, N) * N);
			// b. Recompute B from N and T
			B = cross(N, T);

			return mat3x3(T, B, N);
		}

		inline void populateTextureFrameAttributes(std::vector<VertexAttributes> &vertexData)
		{
			size_t triangleCount = vertexData.size() / 3;
			// We compute the local texture frame per triangle
			for (size_t t = 0; t < triangleCount; ++t)
			{
				VertexAttributes *v = &vertexData[3 * t];

				for (int k = 0; k < 3; ++k)
				{
					mat3x3 TBN = computeTBN(v, v[k].normal);
					v[k].tangent = TBN[0];
					v[k].bitangent = TBN[1];
				}
			}
		}

		inline bool loadGeometryFromObj(const path &path, std::vector<VertexAttributes> &vertexData)
		{
			tinyobj::attrib_t attrib;
			std::vector<tinyobj::shape_t> shapes;
			std::vector<tinyobj::material_t> materials;

			std::string warn;
			std::string err;

			// Call the core loading procedure of TinyOBJLoader
			bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str());

			// Check errors
			if (!warn.empty())
			{
				std::cout << warn << std::endl;
			}

			if (!err.empty())
			{
				std::cerr << err << std::endl;
			}

			if (!ret)
			{
				return false;
			}

			// Filling in vertexData:
			vertexData.clear();
			for (const auto &shape : shapes)
			{
				size_t offset = vertexData.size();
				vertexData.resize(offset + shape.mesh.indices.size());

				for (size_t i = 0; i < shape.mesh.indices.size(); ++i)
				{
					const tinyobj::index_t &idx = shape.mesh.indices[i];

					vertexData[offset + i].position = {
							attrib.vertices[3 * idx.vertex_index + 0],
							-attrib.vertices[3 * idx.vertex_index + 2],
							attrib.vertices[3 * idx.vertex_index + 1]};

					vertexData[offset + i].normal = {
							attrib.normals[3 * idx.normal_index + 0],
							-attrib.normals[3 * idx.normal_index + 2],
							attrib.normals[3 * idx.normal_index + 1]};

					vertexData[offset + i].color = {
							attrib.colors[3 * idx.vertex_index + 0],
							attrib.colors[3 * idx.vertex_index + 1],
							attrib.colors[3 * idx.vertex_index + 2]};

					vertexData[offset + i].uv = {
							attrib.texcoords[2 * idx.texcoord_index + 0],
							1 - attrib.texcoords[2 * idx.texcoord_index + 1]};
				}
			}

			populateTextureFrameAttributes(vertexData);

			return true;
		}

		template <typename position_normal_attributes>
		inline std::tuple<
				std::vector<uint32_t>,
				std::vector<position_normal_attributes>>
		load_attributes(
				const tinyobj::attrib_t &attrib,
				const std::vector<tinyobj::shape_t> &shapes,
				const std::vector<tinyobj::material_t> &materials)
		{
			std::vector<position_normal_attributes> vertex_data(attrib.vertices.size() / 3);
			std::vector<uint32_t> face_ids;
			std::cout << "loading positions" << std::endl;
			for (int i = 0; i < vertex_data.size(); i++)
			{
				vertex_data[i].normal = {0.0, 0.0, 0.0};
				vertex_data[i].position = {
						attrib.vertices[3 * i + 0],
						-attrib.vertices[3 * i + 1],
						attrib.vertices[3 * i + 2]};
			}
			std::cout << "loading indices" << std::endl;
			for (const auto &shape : shapes)
			{
				// we're using triangulate flag, so always have
				size_t offset = face_ids.size();
				face_ids.resize(offset + shape.mesh.indices.size());
				std::cout << "pffset: " << offset << std::endl;
				for (int i = 0; i < shape.mesh.num_face_vertices.size(); i++)
				{
					if (shape.mesh.num_face_vertices[i] != 3)
					{
						std::cout << "non-triangle face detected" << std::endl;
						std::cout << shape.mesh.num_face_vertices[i] << std::endl;
						// exit(0);
					}
				}

				std::cout << "shape mesh indices divisible by 3: " << shape.mesh.indices.size() % 3 << std::endl;
				for (size_t i = 0; i < shape.mesh.indices.size(); i += 3)
				{
					const int &id0 = shape.mesh.indices[i + 0].vertex_index;
					const int &id1 = shape.mesh.indices[i + 1].vertex_index;
					const int &id2 = shape.mesh.indices[i + 2].vertex_index;
					face_ids[offset + i + 0] = id0;
					face_ids[offset + i + 1] = id1;
					face_ids[offset + i + 2] = id2;

					vec3 p0 = vertex_data[id0].position;
					vec3 p1 = vertex_data[id1].position;
					vec3 p2 = vertex_data[id2].position;

					// calc the edge vectors
					vec3 e0 = p1 - p0;
					vec3 e1 = p2 - p0;
					vec3 N = cross(e0, e1);

					vertex_data[id0].normal += N;
					vertex_data[id1].normal += N;
					vertex_data[id2].normal += N;

					// face_ids[offset + i + 0] = 3 * id0;
					// face_ids[offset + i + 1] = 3 * id1;
					// face_ids[offset + i + 2] = 3 * id2;
				}
			}

			for (int i = 0; i < vertex_data.size(); i++)
			{
				vertex_data[i].normal = normalize(vertex_data[i].normal);
			}
			return {face_ids, vertex_data};
		}

		template <typename ATTRIBUTES>
		inline std::tuple<
				std::vector<uint32_t>,
				std::vector<ATTRIBUTES>>
		load_geometry_from_obj(const path &path)
		{
			tinyobj::attrib_t attrib;
			std::vector<tinyobj::shape_t> shapes;
			std::vector<tinyobj::material_t> materials;

			std::string warn;
			std::string err;
			// Call the core loading procedure of TinyOBJLoader
			bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.string().c_str());

			// Check errors
			if (!warn.empty())
				std::cout << warn << std::endl;

			if (!err.empty())
				std::cerr << err << std::endl;

			if (!ret)
				return std::tuple<
						std::vector<uint32_t>,
						std::vector<ATTRIBUTES>>();

			// Filling in vertexData:
			return load_attributes<ATTRIBUTES>(attrib, shapes, materials);
		};

		// Auxiliary function for loadTexture
		inline void writeMipMaps(
				wgpu::Device device,
				wgpu::Texture texture,
				wgpu::Extent3D textureSize,
				uint32_t mipLevelCount,
				const unsigned char *pixelData)
		{
			wgpu::Queue queue = device.getQueue();

			// Arguments telling which part of the texture we upload to
			wgpu::ImageCopyTexture destination;
			destination.texture = texture;
			destination.origin = {0, 0, 0};
			destination.aspect = wgpu::TextureAspect::All;

			// Arguments telling how the C++ side pixel memory is laid out
			wgpu::TextureDataLayout source;
			source.offset = 0;

			// Create image data
			wgpu::Extent3D mipLevelSize = textureSize;
			std::vector<unsigned char> previousLevelPixels;
			wgpu::Extent3D previousMipLevelSize;
			for (uint32_t level = 0; level < mipLevelCount; ++level)
			{
				// Pixel data for the current level
				std::vector<unsigned char> pixels(4 * mipLevelSize.width * mipLevelSize.height);
				if (level == 0)
				{
					// We cannot really avoid this copy since we need this
					// in previousLevelPixels at the next iteration
					memcpy(pixels.data(), pixelData, pixels.size());
				}
				else
				{
					// Create mip level data
					for (uint32_t i = 0; i < mipLevelSize.width; ++i)
					{
						for (uint32_t j = 0; j < mipLevelSize.height; ++j)
						{
							unsigned char *p = &pixels[4 * (j * mipLevelSize.width + i)];
							// Get the corresponding 4 pixels from the previous level
							unsigned char *p00 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 0))];
							unsigned char *p01 = &previousLevelPixels[4 * ((2 * j + 0) * previousMipLevelSize.width + (2 * i + 1))];
							unsigned char *p10 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 0))];
							unsigned char *p11 = &previousLevelPixels[4 * ((2 * j + 1) * previousMipLevelSize.width + (2 * i + 1))];
							// Average
							p[0] = (p00[0] + p01[0] + p10[0] + p11[0]) / 4;
							p[1] = (p00[1] + p01[1] + p10[1] + p11[1]) / 4;
							p[2] = (p00[2] + p01[2] + p10[2] + p11[2]) / 4;
							p[3] = (p00[3] + p01[3] + p10[3] + p11[3]) / 4;
						}
					}
				}

				// Upload data to the GPU texture
				destination.mipLevel = level;
				source.bytesPerRow = 4 * mipLevelSize.width;
				source.rowsPerImage = mipLevelSize.height;
				queue.writeTexture(destination, pixels.data(), pixels.size(), source, mipLevelSize);

				previousLevelPixels = std::move(pixels);
				previousMipLevelSize = mipLevelSize;
				mipLevelSize.width /= 2;
				mipLevelSize.height /= 2;
			}

			queue.release();
		}

		// Load an image from a standard image file into a new texture object
		// NB: The texture must be destroyed after use
		inline wgpu::Texture loadTexture(const path &path, wgpu::Device device, wgpu::TextureView *pTextureView = nullptr)
		{
			int width, height, channels;
			unsigned char *pixelData = stbi_load(path.string().c_str(), &width, &height, &channels, 4 /* force 4 channels */);
			// If data is null, loading failed.
			if (nullptr == pixelData)
				return nullptr;

			// Use the width, height, channels and data variables here
			wgpu::TextureDescriptor textureDesc;
			textureDesc.dimension = wgpu::TextureDimension::_2D;
			textureDesc.format = wgpu::TextureFormat::RGBA8Unorm; // by convention for bmp, png and jpg file. Be careful with other formats.
			textureDesc.size = {(unsigned int)width, (unsigned int)height, 1};
			textureDesc.mipLevelCount = bit_width(std::max(textureDesc.size.width, textureDesc.size.height));
			textureDesc.sampleCount = 1;
			textureDesc.usage = lewitt::flags::texture::read;

			textureDesc.viewFormatCount = 0;
			textureDesc.viewFormats = nullptr;
			wgpu::Texture texture = device.createTexture(textureDesc);

			// Upload data to the GPU texture
			writeMipMaps(device, texture, textureDesc.size, textureDesc.mipLevelCount, pixelData);

			stbi_image_free(pixelData);
			// (Do not use data after this)

			if (pTextureView)
			{
				wgpu::TextureViewDescriptor textureViewDesc;
				textureViewDesc.aspect = wgpu::TextureAspect::All;
				textureViewDesc.baseArrayLayer = 0;
				textureViewDesc.arrayLayerCount = 1;
				textureViewDesc.baseMipLevel = 0;
				textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
				textureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
				textureViewDesc.format = textureDesc.format;
				*pTextureView = texture.createView(textureViewDesc);
			}

			return texture;
		}

		inline std::pair<wgpu::Texture, wgpu::TextureView> loadTextureAndView(const path &path, wgpu::Device device)
		{
			int width, height, channels;
			unsigned char *pixelData = stbi_load(path.string().c_str(), &width, &height, &channels, 4 /* force 4 channels */);
			// If data is null, loading failed.
			if (nullptr == pixelData)
				return {nullptr, nullptr};

			// Use the width, height, channels and data variables here
			wgpu::TextureDescriptor textureDesc;
			textureDesc.dimension = wgpu::TextureDimension::_2D;
			textureDesc.format = wgpu::TextureFormat::RGBA8Unorm; // by convention for bmp, png and jpg file. Be careful with other formats.
			textureDesc.size = {(unsigned int)width, (unsigned int)height, 1};
			textureDesc.mipLevelCount = bit_width(std::max(textureDesc.size.width, textureDesc.size.height));
			textureDesc.sampleCount = 1;
			textureDesc.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
			textureDesc.viewFormatCount = 0;
			textureDesc.viewFormats = nullptr;
			wgpu::Texture texture = device.createTexture(textureDesc);

			// Upload data to the GPU texture
			writeMipMaps(device, texture, textureDesc.size, textureDesc.mipLevelCount, pixelData);

			stbi_image_free(pixelData);
			// (Do not use data after this)
			wgpu::TextureViewDescriptor textureViewDesc;
			textureViewDesc.aspect = wgpu::TextureAspect::All;
			textureViewDesc.baseArrayLayer = 0;
			textureViewDesc.arrayLayerCount = 1;
			textureViewDesc.baseMipLevel = 0;
			textureViewDesc.mipLevelCount = textureDesc.mipLevelCount;
			textureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
			textureViewDesc.format = textureDesc.format;

			return {texture, texture.createView(textureViewDesc)};
		}

		inline std::pair<wgpu::Texture, wgpu::TextureView>
		createEmptyStorageTextureAndView(
				const uint32_t &width, const uint32_t &height,
				wgpu::Device device, const std::string &label = "output")
		{

			wgpu::TextureDescriptor textureDesc;
			textureDesc.dimension = wgpu::TextureDimension::_2D;
			textureDesc.format = wgpu::TextureFormat::RGBA8Unorm;
			textureDesc.size = {width, height, 1};
			textureDesc.sampleCount = 1;
			textureDesc.viewFormatCount = 0;
			textureDesc.viewFormats = nullptr;
			textureDesc.mipLevelCount = 1;
			textureDesc.label = label.c_str();
			textureDesc.usage = lewitt::flags::texture::write;

			wgpu::TextureViewDescriptor textureViewDesc;
			textureViewDesc.aspect = wgpu::TextureAspect::All;
			textureViewDesc.baseArrayLayer = 0;
			textureViewDesc.arrayLayerCount = 1;
			textureViewDesc.dimension = wgpu::TextureViewDimension::_2D;
			textureViewDesc.format = wgpu::TextureFormat::RGBA8Unorm;
			textureViewDesc.mipLevelCount = 1;
			textureViewDesc.baseMipLevel = 0;

			wgpu::Texture output_tex = device.createTexture(textureDesc);
			textureViewDesc.label = label.c_str();
			wgpu::TextureView output_tex_view = output_tex.createView(textureViewDesc);
			return {output_tex, output_tex_view};
		}


	}
}