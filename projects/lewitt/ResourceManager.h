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

class ResourceManager
{
public:
	// (Just aliases to make notations lighter)
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
	// Load a shader from a WGSL file into a new shader module
	static wgpu::ShaderModule loadShaderModule(const path &path, wgpu::Device device);

	// Load an 3D mesh from a standard .obj file into a vertex data buffer
	static bool loadGeometryFromObj(const path &path, std::vector<VertexAttributes> &vertexData);

	template <typename position_normal_attributes>
	inline static std::tuple<
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

				face_ids[offset + i + 0] = 3 * id0;
				face_ids[offset + i + 1] = 3 * id1;
				face_ids[offset + i + 2] = 3 * id2;
			}
		}

		for (int i = 0; i < vertex_data.size(); i++)
		{
			vertex_data[i].normal = normalize(vertex_data[i].normal);
		}
		return {face_ids, vertex_data};
	}

	template <typename ATTRIBUTES>
	inline static std::tuple<
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

	// Load an image from a standard image file into a new texture object
	// NB: The texture must be destroyed after use
	static wgpu::Texture loadTexture(const path &path, wgpu::Device device, wgpu::TextureView *pTextureView = nullptr);
	static std::pair<wgpu::Texture, wgpu::TextureView> loadTextureAndView(const path &path, wgpu::Device device);
	static std::pair<wgpu::Texture, wgpu::TextureView> createEmptyStorageTextureAndView(const int &width, const int &height, wgpu::Device device, const std::string &label = "output");

private:
	// Compute the TBN local to a triangle face from its corners and return it as
	// a matrix whose columns are the T, B and N vectors.
	static mat3x3 computeTBN(const VertexAttributes corners[3], const vec3 &expectedN);

	// Compute Tangent and Bitangent attributes from the normal and UVs.
	static void populateTextureFrameAttributes(std::vector<VertexAttributes> &vertexData);
};
