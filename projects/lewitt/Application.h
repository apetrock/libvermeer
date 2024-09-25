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

// named after Sol LeWitt, minimalist artist.
#pragma once

#include <webgpu/webgpu.hpp>
#include <glm/glm.hpp>

#include <array>
#include "lewitt/common.h"
#include "lewitt/doables.hpp"
#include "lewitt/scene.hpp"
// Forward declare
struct GLFWwindow;

class Application
{
public:
	// A function called only once at the beginning. Returns false is init failed.
	bool onInit();

	// A function called at each frame, guaranteed never to be called before `onInit`.
	void onFrame(uint frame);
	void onCompute();

	// A function called only once at the very end.
	void onFinish();

	// A function that tells if the application is still running.
	bool isRunning();

	// A function called when the window is resized.
	void onResize();

	// Mouse events
	void onMouseMove(double xpos, double ypos);
	void onMouseButton(int button, int action, int mods);
	void onScroll(double xoffset, double yoffset);

private:
	bool initWindowAndDevice();
	void terminateWindowAndDevice();

	bool initSwapChain();
	void terminateSwapChain();

	bool initDepthBuffer();
	void terminateDepthBuffer();
  
	bool initUniforms();

	bool initTestCompute();

	bool initLightingUniforms();
	void updateLightingUniforms();

	bool initSphere();
	bool initCylinder();
	bool initCapsule();

	bool initBunny();
	bool initBunnyInstanced();
	bool initRenderables();

	void logRandLines(uint N);

	bool initGui();																			// called in onInit
	void terminateGui();																// called in onFinish
	void updateGui(wgpu::RenderPassEncoder renderPass); // called in onFrame

private:
	// (Just aliases to make notations lighter)
	using mat4x4 = glm::mat4x4;
	using vec4 = glm::vec4;
	using vec3 = glm::vec3;
	using vec2 = glm::vec2;

	/**
	 * The same structure as in the shader, replicated in C++
	 */
	//

	// Window and Device
	GLFWwindow *m_window = nullptr;
	wgpu::Instance m_instance = nullptr;
	wgpu::Surface m_surface = nullptr;
	wgpu::Device m_device = nullptr;
	wgpu::Queue m_queue = nullptr;
	wgpu::TextureFormat m_swapChainFormat = wgpu::TextureFormat::Undefined;
	// Keep the error callback alive
	std::unique_ptr<wgpu::ErrorCallback> m_errorCallbackHandle;

	// Swap Chain
	wgpu::SwapChain m_swapChain = nullptr;

	// Depth Buffer
	wgpu::TextureFormat m_depthTextureFormat = wgpu::TextureFormat::Depth24Plus;
	wgpu::Texture m_depthTexture = nullptr;
	wgpu::TextureView m_depthTextureView = nullptr;
	//std::vector<lewitt::doables::renderable::ptr> _renderables;
	//std::vector<lewitt::doables::computable::ptr> _computables;
	lewitt::render_scene::ptr _render_scene;
	lewitt::compute_scene::ptr _compute_scene;
	
	//lewitt::doables::renderable::ptr _bunny;
	//lewitt::doables::renderable::ptr _bunny_instanced;
	//lewitt::doables::renderable::ptr _sphere;
	//lewitt::doables::renderable::ptr _cylinder;
	//lewitt::doables::renderable::ptr _capsule;
	
};
