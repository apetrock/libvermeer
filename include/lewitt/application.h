#pragma once

#include <webgpu/webgpu.hpp>
#include <glm/glm.hpp>

#include <array>
#include "common.h"
#include "doables.hpp"
#include "scene.hpp"

struct GLFWwindow;

namespace lewitt
{
	// Forward declare
  class app_runner {

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

	bool init_scenes();
	bool initGui();																			// called in onInit
	void terminateGui();																// called in onFinish
	void updateGui(wgpu::RenderPassEncoder renderPass); // called in onFrame

private:
	// (Just aliases to make notations lighter)
	using mat4x4 = glm::mat4x4;
	using vec4 = glm::vec4;
	using vec3 = glm::vec3;
	using vec2 = glm::vec2;


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
	lewitt::render_scene::ptr _render_scene;
	lewitt::compute_scene::ptr _compute_scene;

	//function pointser for before compute, before render, imgui, etc
	std::function<void(int)> _init;
	std::function<void(int)> _before_compute;
	std::function<void(int)> _before_render;
	std::function<void(wgpu::RenderPassEncoder)> _update_gui;

  };
} // namespace lewitt