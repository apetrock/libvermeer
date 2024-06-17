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

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Application
{
public:
	// A function called only once at the beginning. Returns false is init failed.
	bool onInit();

	// A function called only once at the very end.
	void onFinish();

	// A function that tells if the application is still running.
	bool isRunning();

	// A function called at each frame, guarantied never to be called before `onInit`.
	void onFrame();

	// Where the GPU computation is actually issued
	void onCompute();

	// A function that tells if the compute pass should be executed
	// (i.e., when filter parameters changed)
	bool shouldCompute();

public:
	// A function called when we should draw the GUI.
	void onGui(wgpu::RenderPassEncoder renderPass);

	// A function called when the window is resized.
	void onResize();
	// for a path tracer you have a buffer of rays and a buffer of hits
	// the rays are generated from the camera and the hits are the result of the rays hitting the scene
	// because rays will get reused, we need need to set
	struct ray
	{
		glm::vec3 origin;
		uint32_t pad;
		glm::vec3 direction;
		uint32_t id;
	};

	static_assert(sizeof(ray) % 16 == 0);

private:
	// Detailed steps
	bool initDevice();
	void terminateDevice();

	bool initWindow();
	void terminateWindow();

	void initSwapChain();
	void terminateSwapChain();

	void initGui();
	void terminateGui();

	void initBuffers();
	void terminateBuffers();

	void initTextures();
	void terminateTextures();

	void initTextureViews();
	void terminateTextureViews();

	void initBindGroup();
	void terminateBindGroup();

	void initBindGroupLayout();
	void terminateBindGroupLayout();

	void initComputePipeline();
	void terminateComputePipeline();

private:
	uint32_t m_bufferSize;
	GLFWwindow *m_window = nullptr;
	wgpu::Surface m_surface = nullptr;
	wgpu::Instance m_instance = nullptr;
	wgpu::Adapter m_adapter = nullptr;
	wgpu::Device m_device = nullptr;
	wgpu::Queue m_queue = nullptr;
	wgpu::TextureFormat m_swapChainFormat = wgpu::TextureFormat::Undefined;
	wgpu::SwapChainDescriptor m_swapChainDesc;
	wgpu::SwapChain m_swapChain = nullptr;
	wgpu::BindGroup m_bindGroup = nullptr;
	wgpu::BindGroupLayout m_bindGroupLayout = nullptr;
	wgpu::PipelineLayout m_pipelineLayout = nullptr;
	wgpu::ComputePipeline m_pipeline = nullptr;
	std::unique_ptr<wgpu::ErrorCallback> m_uncapturedErrorCallback;
	std::unique_ptr<wgpu::DeviceLostCallback> m_deviceLostCallback;

	bool m_shouldCompute = true;
	wgpu::Buffer m_uniformBuffer = nullptr;
	wgpu::Texture m_inputTexture = nullptr;
	wgpu::Texture m_outputTexture = nullptr;
	wgpu::TextureView m_inputTextureView = nullptr;
	wgpu::TextureView m_outputTextureView = nullptr;

	// Computed from the Parameters
	struct Uniforms
	{
		glm::quat orientation = glm::quat(1.0, 0.0, 0.0, 0.0);
		uint32_t width = 0;
		uint32_t height = 0;
		float p0;
		float p1;
	};

	static_assert(sizeof(Uniforms) % 16 == 0);
	Uniforms m_uniforms;

	// Similar to parameters, but do not trigger recomputation of the effect
	struct Settings
	{
		float scale = 0.5f;
	};

	wgpu::Buffer m_rayBuffer = nullptr;
	Settings m_settings;
};
