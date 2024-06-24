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
#include "common.h"

#include "uniforms.hpp"
#include "bindings.hpp"
#include "shaders.hpp"
#include "buffers.hpp"
#include "vertex_formats.hpp"
#include "doables.hpp"
// Forward declare
struct GLFWwindow;

class Application
{
public:
	// A function called only once at the beginning. Returns false is init failed.
	bool onInit();

	// A function called at each frame, guaranteed never to be called before `onInit`.
	void onFrame();

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

	bool initRenderPipeline();
	void terminateRenderPipeline();

	bool initTextures();

	bool initGeometry();

	bool initUniforms();

	bool initLightingUniforms();
	void updateLightingUniforms();

	bool initBindGroupLayout();

	bool initBindGroup();
	bool initRenderables();

	void updateProjectionMatrix();
	void updateViewMatrix();
	void updateDragInertia();

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
	struct MyUniforms
	{
		// We add transform matrices
		mat4x4 projectionMatrix;
		mat4x4 viewMatrix;
		mat4x4 modelMatrix;
		vec4 color;
		vec3 cameraWorldPosition;
		float time;
	};
	// Have the compiler check byte alignment
	static_assert(sizeof(MyUniforms) % 16 == 0);

	struct LightingUniforms
	{
		std::array<vec4, 2> directions;
		std::array<vec4, 2> colors;

		// Material properties
		float hardness = 32.0f;
		float kd = 1.0f;
		float ks = 0.5f;

		float _pad[1];
	};

	//

	static_assert(sizeof(LightingUniforms) % 16 == 0);

	struct CameraState
	{
		// angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
		// angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
		vec2 angles = {0.8f, 0.5f};
		// zoom is the position of the camera along its local forward axis, affected by the scroll wheel
		float zoom = -1.2f;
	};

	struct DragState
	{
		// Whether a drag action is ongoing (i.e., we are between mouse press and mouse release)
		bool active = false;
		// The position of the mouse at the beginning of the drag action
		vec2 startMouse;
		// The camera state at the beginning of the drag action
		CameraState startCameraState;

		// Constant settings
		float sensitivity = 0.01f;
		float scrollSensitivity = 0.1f;

		// Inertia
		vec2 velocity = {0.0, 0.0};
		vec2 previousDelta;
		float intertia = 0.9f;
	};

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

	// Render Pipeline
	wgpu::ShaderModule m_shaderModule = nullptr;
	wgpu::RenderPipeline m_pipeline = nullptr;

	// object seems to have a pipeline,
	lewitt::doables::renderable::ptr _cylinder;
	lewitt::shaders::NCUVTB::ptr _ncuvtb_shader;
	lewitt::buffers::buffer::ptr _geometry;
	int _u_id = -1, _u_lighting_id = -1;
	lewitt::bindings::group::ptr _bind_group = nullptr;

	int m_vertexCount = 0;

	// Bind Group

	CameraState m_cameraState;
	DragState m_drag;
};
