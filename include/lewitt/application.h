#pragma once

#include <webgpu/webgpu.hpp>
#include <glm/glm.hpp>

#include <array>
#include <functional>
#include <memory>
#include "common.h"
#include "doables.hpp"
#include "gbuffer_pipeline.hpp"
#include "gpu_session.hpp"
#include "nodes.hpp"
#include "render_targets.hpp"
#include "resource_handles.hpp"
#include "scene.hpp"

struct GLFWwindow;

namespace lewitt
{
	// Forward declare
  class app_runner {

public:
	// A function called only once at the beginning. Returns false is init failed.
	bool onInit(uint32_t width = 640, uint32_t height = 480);

  // A function called at each frame, guaranteed never to be called before `onInit`.
  void onFrame(uint frame);
  void onCompute();

  // Optional host hooks for parent projects (e.g. libgaudi Duchamp demos).
  void set_init_callback(std::function<bool()> callback);
  void set_frame_callback(std::function<void(uint)> callback);
  void set_before_render_callback(std::function<void(uint)> callback);
  void set_project_renderer(project_builder builder);

  wgpu::Device device() const { return m_device; }
  wgpu::Queue queue() const { return m_queue; }
  wgpu::TextureFormat color_format() const { return m_swapChainFormat; }
  wgpu::TextureFormat depth_format() const { return m_depthTextureFormat; }
  lewitt::render_scene::ptr render_scene() const { return _render_scene; }
  void add_gbuffer_renderable(lewitt::doables::g_buffer_renderable::ptr renderable);

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

	bool initGbufferPipeline();
	void terminateGbufferPipeline();

	bool init_scenes();
	bool initGui();																			// called in onInit
	void terminateGui();																// called in onFinish
	void updateGui(wgpu::RenderPassEncoder renderPass); // called in onFrame
	gpu_context make_gpu_context() const;

private:
	// (Just aliases to make notations lighter)
	using mat4x4 = glm::mat4x4;
	using vec4 = glm::vec4;
	using vec3 = glm::vec3;
	using vec2 = glm::vec2;


	// Window and Device
	GLFWwindow *m_window = nullptr;
	uint32_t m_windowWidth = 640;
	uint32_t m_windowHeight = 480;
	wgpu::Instance m_instance = nullptr;
	wgpu::Surface m_surface = nullptr;
	wgpu::Device m_device = nullptr;
	wgpu::Queue m_queue = nullptr;
	wgpu::TextureFormat m_swapChainFormat = wgpu::TextureFormat::Undefined;
	// Keep the error callback alive
	std::unique_ptr<wgpu::ErrorCallback> m_errorCallbackHandle;

	// Swap Chain
	wgpu::SwapChain m_swapChain = nullptr;

	// Depth / G-buffer pipeline
	wgpu::TextureFormat m_depthTextureFormat = resources::depth_attachment::format();
	pipeline::gbuffer_pass m_gbufferPass;
	nodes::passthrough_visualizer::ptr m_passthrough;
	resources::texture_input<resources::position_sampled> m_passthroughInputs;
	std::vector<lewitt::doables::g_buffer_renderable::ptr> m_gbufferRenderables;
	render_targets::extent2d m_framebufferExtent{};
	std::unique_ptr<project_renderer> m_projectRenderer;
	project_builder m_projectBuilder;
	lewitt::render_scene::ptr _render_scene;
	lewitt::compute_scene::ptr _compute_scene;

  //function pointser for before compute, before render, imgui, etc
  std::function<bool()> _init_callback;
  std::function<void(uint)> _frame_callback;
  std::function<void(uint)> _before_render_callback;
  std::function<void(int)> _init;
  std::function<void(int)> _before_compute;
  std::function<void(int)> _before_render;
  std::function<void(wgpu::RenderPassEncoder)> _update_gui;

  };
} // namespace lewitt