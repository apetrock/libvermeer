#include "lewitt/application.h"

#include "lewitt/resources.hpp"
#include "lewitt/draw_primitives.hpp"
#include "lewitt/geometry_logger.h"
#include "lewitt/passes.hpp"

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

	bool app_runner::onInit()
	{

		if (!initWindowAndDevice())
			return false;
		if (!initSwapChain())
			return false;
		if (!initDepthBuffer())
			return false;

		if (!init_scenes())
			return false;

		if (!initGui())
			return false;
		return true;
	}

	void app_runner::onFrame(uint frame)
	{
		onCompute();

		glfwPollEvents();
		_render_scene->update();
		_render_scene->update_uniforms(m_queue);
		//_cylinder_normal_texture->get_bindings()->get_uniform_binding(_u_id)->set_member("time", static_cast<float>(glfwGetTime()));
		// m_uniforms.time = static_cast<float>(glfwGetTime());
		// m_queue.writeBuffer(m_uniformBuffer, offsetof(MyUniforms, time), &m_uniforms.time, sizeof(MyUniforms::time));
		passes::render(m_swapChain, m_device, m_depthTextureView,
									 [&](wgpu::RenderPassEncoder &render_pass, wgpu::Device &device)
									 {
										 _render_scene->render(render_pass, device);
										 // We add the GUI drawing commands to the render pass
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
		terminateDepthBuffer();
		terminateSwapChain();
		terminateWindowAndDevice();
	}

	bool app_runner::isRunning()
	{
		return !glfwWindowShouldClose(m_window);
	}

	void app_runner::onResize()
	{
		// Terminate in reverse order
		terminateDepthBuffer();
		terminateSwapChain();

		// Re-init
		initSwapChain();
		initDepthBuffer();

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
		m_window = glfwCreateWindow(640, 480, "Learn WebGPU", NULL, NULL);
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

	bool app_runner::initDepthBuffer()
	{
		// Get the current size of the window's framebuffer:
		int width, height;
		glfwGetFramebufferSize(m_window, &width, &height);

		// Create the depth texture
		TextureDescriptor depthTextureDesc;
		depthTextureDesc.dimension = TextureDimension::_2D;
		depthTextureDesc.format = m_depthTextureFormat;
		depthTextureDesc.mipLevelCount = 1;
		depthTextureDesc.sampleCount = 1;
		depthTextureDesc.size = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
		depthTextureDesc.usage = TextureUsage::RenderAttachment;
		depthTextureDesc.viewFormatCount = 1;
		depthTextureDesc.viewFormats = (WGPUTextureFormat *)&m_depthTextureFormat;
		m_depthTexture = m_device.createTexture(depthTextureDesc);
		std::cout << "Depth texture: " << m_depthTexture << std::endl;

		// Create the view of the depth texture manipulated by the rasterizer
		TextureViewDescriptor depthTextureViewDesc;
		depthTextureViewDesc.aspect = TextureAspect::DepthOnly;
		depthTextureViewDesc.baseArrayLayer = 0;
		depthTextureViewDesc.arrayLayerCount = 1;
		depthTextureViewDesc.baseMipLevel = 0;
		depthTextureViewDesc.mipLevelCount = 1;
		depthTextureViewDesc.dimension = TextureViewDimension::_2D;
		depthTextureViewDesc.format = m_depthTextureFormat;
		m_depthTextureView = m_depthTexture.createView(depthTextureViewDesc);
		std::cout << "Depth texture view: " << m_depthTextureView << std::endl;

		return m_depthTextureView != nullptr;
	}

	void app_runner::terminateDepthBuffer()
	{
		m_depthTextureView.release();
		m_depthTexture.destroy();
		m_depthTexture.release();
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

		_render_scene->init_camera(m_window, m_device);
		_render_scene->init_lighting(m_device);

		logRandLines(100);

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
		ImGui_ImplWGPU_Init(m_device, 3, m_swapChainFormat, m_depthTextureFormat);
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

}