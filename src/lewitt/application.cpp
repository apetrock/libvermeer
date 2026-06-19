#include "lewitt/application.h"

#include "lewitt/resources.hpp"
#include "lewitt/draw_primitives.hpp"
#include "lewitt/geometry_logger.h"
#include "lewitt/passes.hpp"
#include "lewitt/gbuffer_pipeline.hpp"
#include "lewitt/gpu_session.hpp"
#include "lewitt/present_target.hpp"

#include "lewitt/buffers.hpp"
#include "lewitt/buffer_ops.hpp"

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/polar_coordinates.hpp>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include <iostream>

#include <random>
#include <cassert>
#include <filesystem>
#include <sstream>
#include <string>
#include <array>

constexpr float PI = 3.14159265358979323846f;

// Custom ImGui widgets
/*
namespace ImGui
{
	bool DragDirection(const char *label, glm::vec4 &direction)
	{
		glm::vec2 angles = glm::degrees(glm::polar(glm::vec3(direction)));
		bool changed = ImGui::DragFloat2(label, glm::value_ptr(angles));
		direction = glm::vec4(glm::euclidean(glm::radians(angles)), direction.w);
		return changed;
	}
} // namespace ImGui
*/
namespace lewitt
{

	using namespace wgpu;
	using VertexAttributes = resources::VertexAttributes;

	///////////////////////////////////////////////////////////////////////////////
	// Public methods

	bool app_runner::onInit(uint32_t width, uint32_t height)
	{
		m_windowWidth = width;
		m_windowHeight = height;

		if (!initWindowAndDevice())
			return false;
		if (!initSwapChain())
			return false;

		{
			int width = 0;
			int height = 0;
			glfwGetFramebufferSize(m_window, &width, &height);
			m_framebufferExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
			if (!m_framebufferExtent.width || !m_framebufferExtent.height) {
				m_framebufferExtent = {m_windowWidth, m_windowHeight};
			}
		}

		if (!init_scenes())
			return false;

		if (m_projectBuilder) {
			auto ctx = make_gpu_context();
			m_projectRenderer = m_projectBuilder(ctx);
			if (!m_projectRenderer || !m_projectRenderer->init(ctx)) {
				return false;
			}
		} else if (!initGbufferPipeline()) {
			return false;
		}

		if (!initGui())
			return false;
		return true;
	}

	void app_runner::set_init_callback(std::function<bool()> callback)
	{
		_init_callback = std::move(callback);
	}

	void app_runner::set_frame_callback(std::function<void(uint)> callback)
	{
		_frame_callback = std::move(callback);
	}

	void app_runner::set_before_render_callback(std::function<void(uint)> callback)
	{
		_before_render_callback = std::move(callback);
	}

	void app_runner::set_project_renderer(project_builder builder)
	{
		m_projectBuilder = std::move(builder);
	}

	void app_runner::add_gbuffer_renderable(doables::g_buffer_renderable::ptr renderable)
	{
		if (!renderable) {
			return;
		}
		m_gbufferRenderables.push_back(std::move(renderable));
	}

	void app_runner::onFrame(uint frame)
	{
		if (_frame_callback)
			_frame_callback(frame);

		onCompute();

		glfwPollEvents();

		if (m_projectRenderer) {
			auto ctx = make_gpu_context();
			m_projectRenderer->update(ctx, frame);
			m_projectRenderer->render(ctx, [&](wgpu::RenderPassEncoder &render_pass) {
				updateGui(render_pass);
			});
			return;
		}

		_render_scene->update();
		_render_scene->update_uniforms(m_queue);

		m_gbufferPass.execute(m_device, [&](wgpu::RenderPassEncoder &render_pass, wgpu::Device &device) {
			if (_before_render_callback)
				_before_render_callback(frame);
		});

		render_targets::frame_attachments swapchain_attachments{};
		passes::render(m_swapChain, m_device, swapchain_attachments,
									 [&](wgpu::RenderPassEncoder &render_pass, wgpu::Device &device) {
										 if (m_passthrough) {
											 m_passthrough->draw(render_pass, device, m_queue, m_gbufferPass.pool());
										 }
										 updateGui(render_pass);
									 });
	}

	#include <random>
	GLM_TYPEDEFS;
	std::vector<vec3> gen_rand_offsets(int N)
	{
		// use std::randomg device w/mersein twister
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(-1.0, 1.0);
		std::vector<vec3> offsets;
		for (int i = 0; i < N; i++)
		{
			offsets.push_back(vec3(dis(gen), dis(gen), dis(gen)));
		}
		return offsets;
	}

	void app_runner::onCompute()
	{
		passes::compute(m_device, [&](wgpu::ComputePassEncoder &compute_pass, wgpu::Device &device)
										{ _compute_scene->compute(compute_pass, device); });
		
		/*
		could write creates: 
			compute_create, //copy read
			copy_compute_create, //copy
			write_compute_create, //write
			const_compute_create, //read only
		*/
		buffers::buffer::ptr A = buffers::buffer::create();
		buffers::buffer::ptr B = buffers::buffer::create();
		A->set_usage(flags::storage::read);
		B->set_usage(flags::storage::read);
    std::vector<vec3> A_cpu =  gen_rand_offsets(16);
    std::vector<vec3> B_cpu = gen_rand_offsets(16);
    for (int i = 0; i < A_cpu.size(); ++i) {
        std::cout << "A["<<i<<"]: " << A_cpu[i][0] << " ";
    }
    std::cout << std::endl;
    for (int i = 0; i < B_cpu.size(); ++i) {
        std::cout << "B["<<i<<"]: " << B_cpu[i][0] << " ";
    }
    std::cout << std::endl;
		
    for (int i = 0; i < B_cpu.size(); ++i) {
        std::cout << "AB[0]["<<i<<"]: " << A_cpu[i][0] + B_cpu[i][0] << " ";
    }
    std::cout << std::endl;    
    for (int i = 0; i < B_cpu.size(); ++i) {
        std::cout << "AB[1]["<<i<<"]: " << A_cpu[i][1] + B_cpu[i][1] << " ";
    }
    std::cout << std::endl;
    for (int i = 0; i < B_cpu.size(); ++i) {
        std::cout << "AB[2]["<<i<<"]: " << A_cpu[i][2] + B_cpu[i][2] << " ";
    }
    std::cout << std::endl;
    A->write<vec3>(A_cpu, m_device);
		B->write<vec3>(B_cpu, m_device);
		buffers::buffer::ptr C = buffers::add_vec3f(A,B, m_device);
	}

	void app_runner::onFinish()
	{
		terminateGui();
		terminateGbufferPipeline();
		terminateSwapChain();
		terminateWindowAndDevice();
	}

	bool app_runner::isRunning()
	{
		return !glfwWindowShouldClose(m_window);
	}

	void app_runner::onResize()
	{
		terminateSwapChain();
		initSwapChain();

		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);
		m_framebufferExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
		if (!m_framebufferExtent.width || !m_framebufferExtent.height) {
			m_framebufferExtent = {m_windowWidth, m_windowHeight};
		}
		if (m_projectRenderer) {
			auto ctx = make_gpu_context();
			m_projectRenderer->resize(ctx);
			return;
		}
		m_gbufferPass.resize(m_device, m_framebufferExtent);
		resources::wire(m_gbufferPass.outputs().position, m_passthroughInputs.source);
		if (m_passthrough) {
			m_passthrough->set_input(nodes::passthrough_visualizer::input_port::source,
															 m_passthroughInputs);
		}

		_render_scene->update_uniforms(m_queue);
	}

	void app_runner::onMouseMove(double xpos, double ypos)
	{
		_render_scene->camera_move(xpos, ypos, m_queue);
	}

	void app_runner::onMouseButton(int button, int action, int /* modifiers */)
	{
		ImGuiIO &io = ImGui::GetIO();
		if (io.WantCaptureMouse)
		{
			// Don't rotate the camera if the mouse is already captured by an ImGui
			// interaction at this frame.
			return;
		}

		if (button == GLFW_MOUSE_BUTTON_LEFT)
		{
			switch (action)
			{
			case GLFW_PRESS:
				_render_scene->camera_move_start();
				break;
			case GLFW_RELEASE:
				_render_scene->camera_move_end();
				break;
			}
		}
	}

	void app_runner::onScroll(double xoffset, double yoffset)
	{
		_render_scene->camera_scroll(xoffset, yoffset, m_queue);
	}

	///////////////////////////////////////////////////////////////////////////////
	// Private methods

	bool app_runner::initWindowAndDevice()
	{
		m_instance = createInstance(InstanceDescriptor{});
		if (!m_instance)
		{
			std::cerr << "Could not initialize WebGPU!" << std::endl;
			return false;
		}

		if (!glfwInit())
		{
			std::cerr << "Could not initialize GLFW!" << std::endl;
			return false;
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		m_window = glfwCreateWindow(static_cast<int>(m_windowWidth),
		                            static_cast<int>(m_windowHeight), "Learn WebGPU", NULL, NULL);
		if (!m_window)
		{
			std::cerr << "Could not open window!" << std::endl;
			return false;
		}

		std::cout << "Requesting adapter..." << std::endl;
		m_surface = glfwGetWGPUSurface(m_instance, m_window);
		RequestAdapterOptions adapterOpts{};
		adapterOpts.compatibleSurface = m_surface;
		Adapter adapter = m_instance.requestAdapter(adapterOpts);
		std::cout << "Got adapter: " << adapter << std::endl;

		SupportedLimits supportedLimits;
		adapter.getLimits(&supportedLimits);

		std::cout << "Requesting device..." << std::endl;

		RequiredLimits requiredLimits = Default;
		requiredLimits.limits = supportedLimits.limits;
		DeviceDescriptor deviceDesc;
		deviceDesc.label = "My Device";
		deviceDesc.requiredFeaturesCount = 0;
		deviceDesc.requiredLimits = &requiredLimits;
		deviceDesc.defaultQueue.label = "The default queue";
		m_device = adapter.requestDevice(deviceDesc);
		std::cout << "Got device: " << m_device << std::endl;
		// Add an error callback for more debug info
		m_errorCallbackHandle = m_device.setUncapturedErrorCallback([](ErrorType type, char const *message)
																																{
		std::cout << "Device error: type " << type;
		if (message) std::cout << " (message: " << message << ")";
		std::cout << std::endl; });

		m_queue = m_device.getQueue();

#ifdef WEBGPU_BACKEND_WGPU
		m_swapChainFormat = m_surface.getPreferredFormat(adapter);
#else
		m_swapChainFormat = TextureFormat::BGRA8Unorm;
#endif

		// Add window callbacks
		// Set the user pointer to be "this"
		glfwSetWindowUserPointer(m_window, this);
		glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow *window, int, int)
																	 {
		auto that = reinterpret_cast<app_runner*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onResize(); });
		glfwSetCursorPosCallback(m_window, [](GLFWwindow *window, double xpos, double ypos)
														 {
		auto that = reinterpret_cast<app_runner*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseMove(xpos, ypos); });
		glfwSetMouseButtonCallback(m_window, [](GLFWwindow *window, int button, int action, int mods)
															 {
		auto that = reinterpret_cast<app_runner*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseButton(button, action, mods); });
		glfwSetScrollCallback(m_window, [](GLFWwindow *window, double xoffset, double yoffset)
													{
		auto that = reinterpret_cast<app_runner*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onScroll(xoffset, yoffset); });

		adapter.release();
		return m_device != nullptr;
	}

	void app_runner::terminateWindowAndDevice()
	{
		m_queue.release();
		m_device.release();
		m_surface.release();
		m_instance.release();

		glfwDestroyWindow(m_window);
		glfwTerminate();
	}

	bool app_runner::initSwapChain()
	{
		// Get the current size of the window's framebuffer:
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);

		std::cout << "Creating swapchain..." << std::endl;
		SwapChainDescriptor swapChainDesc;
		swapChainDesc.width = static_cast<uint32_t>(width);
		swapChainDesc.height = static_cast<uint32_t>(height);
		swapChainDesc.usage = TextureUsage::RenderAttachment;
		swapChainDesc.format = m_swapChainFormat;
		swapChainDesc.presentMode = PresentMode::Fifo;
		m_swapChain = m_device.createSwapChain(m_surface, swapChainDesc);
		std::cout << "Swapchain: " << m_swapChain << std::endl;
		return m_swapChain != nullptr;
	}

	void app_runner::terminateSwapChain()
	{
		m_swapChain.release();
	}

	bool app_runner::initGbufferPipeline()
	{
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);
		m_framebufferExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
		if (!m_framebufferExtent.width || !m_framebufferExtent.height) {
			m_framebufferExtent = {m_windowWidth, m_windowHeight};
		}

		if (m_gbufferPass.empty()) {
			for (const auto &renderable : m_gbufferRenderables) {
				m_gbufferPass.add(renderable);
				if (_render_scene) {
					_render_scene->bind_camera(renderable);
				}
			}
		}

		const auto outputs = m_gbufferPass.init(m_device, m_framebufferExtent);
		resources::wire(outputs.position, m_passthroughInputs.source);

		m_passthrough = nodes::passthrough_visualizer::create();
		m_passthrough->init(m_device, lewitt::present_target::from_context(
		                                  m_swapChain, m_swapChainFormat, m_framebufferExtent));
		m_passthrough->set_input(nodes::passthrough_visualizer::input_port::source,
														 m_passthroughInputs);
		m_passthrough->set_mode(nodes::passthrough_mode::position);
		return outputs.position.id != render_targets::invalid_target_id;
	}

	void app_runner::terminateGbufferPipeline()
	{
		m_gbufferPass.pool().clear();
		m_passthroughInputs.source.id = render_targets::invalid_target_id;
		m_passthrough.reset();
	}

	std::tuple<vec3, vec3, vec3, float> rand_line()
	{
		// use std::randomg device w/mersein twister
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> rvec(-1.0, 1.0);
		std::uniform_real_distribution<float> rcol(0.0, 1.0);
		vec3 v0(rvec(gen), rvec(gen), rvec(gen));
		vec3 v1(rvec(gen), rvec(gen), rvec(gen));
		vec3 col(rcol(gen), rcol(gen), rcol(gen));
		float r = 0.1 * rcol(gen);
		return {v0, v1, col, r};
	}
	void logRandLines(uint N)
	{
		for (int i = 0; i < N; i++)
		{
			auto [v0, v1, col, r] = rand_line();
			lewitt::logger::geometry::line({v0, v1}, col, r);
		}
	}

	bool app_runner::init_scenes()
	{
		_compute_scene = lewitt::compute_scene::create();

		_render_scene = lewitt::render_scene::create();
		lewitt::logger::geometry::get_instance().debugLines->set_texture_format(m_swapChainFormat, m_depthTextureFormat);
		_render_scene->renderables.push_back(lewitt::logger::geometry::get_instance().debugLines);

		if (_init_callback && !_init_callback())
			return false;

		_render_scene->init_camera(m_window, m_device);
		_render_scene->init_lighting(m_device);

		return true;
	}

	bool app_runner::initGui()
	{
		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::GetIO();

		// Setup Platform/Rendeonrer backends
		ImGui_ImplGlfw_InitForOther(m_window, true);
		// Project-style rendering draws ImGui into a color-only swapchain pass.
		wgpu::TextureFormat imgui_depth = m_depthTextureFormat;
		if (m_projectBuilder) {
			imgui_depth = wgpu::TextureFormat::Undefined;
		}
		ImGui_ImplWGPU_Init(m_device, 3, m_swapChainFormat, imgui_depth);
		return true;
	}

	void app_runner::terminateGui()
	{
		ImGui_ImplGlfw_Shutdown();
		ImGui_ImplWGPU_Shutdown();
	}

	void app_runner::updateGui(RenderPassEncoder renderPass)
	{
		// Start the Dear ImGui frame
		ImGui_ImplWGPU_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Build our UI
		{
			bool changed = false;
			ImGui::Begin("Lighting");
			/*
			changed = ImGui::ColorEdit3("Color #0", glm::value_ptr(m_lightingUniforms.colors[0])) || changed;
			changed = ImGui::DragDirection("Direction #0", m_lightingUniforms.directions[0]) || changed;
			changed = ImGui::ColorEdit3("Color #1", glm::value_ptr(m_lightingUniforms.colors[1])) || changed;
			changed = ImGui::DragDirection("Direction #1", m_lightingUniforms.directions[1]) || changed;
			changed = ImGui::SliderFloat("Hardness", &m_lightingUniforms.hardness, 1.0f, 100.0f) || changed;
			changed = ImGui::SliderFloat("K Diffuse", &m_lightingUniforms.kd, 0.0f, 1.0f) || changed;
			changed = ImGui::SliderFloat("K Specular", &m_lightingUniforms.ks, 0.0f, 1.0f) || changed;
			*/
			ImGui::End();
		}

		// Draw the UI
		ImGui::EndFrame();
		// Convert the UI defined above into low-level drawing commands
		ImGui::Render();
		// Execute the low-level drawing commands on the WebGPU backend
		ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
	}

	gpu_context app_runner::make_gpu_context() const
	{
		gpu_context ctx{};
		ctx.window = m_window;
		ctx.device = m_device;
		ctx.queue = m_queue;
		ctx.swapchain = m_swapChain;
		ctx.swapchain_format = m_swapChainFormat;
		ctx.depth_format = m_depthTextureFormat;
		ctx.extent = m_framebufferExtent;
		ctx.scene = _render_scene;
		return ctx;
	}

}