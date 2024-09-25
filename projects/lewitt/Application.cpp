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

#include "Application.h"
#include "lewitt/resources.hpp"
#include "lewitt/draw_primitives.hpp"
#include "lewitt/geometry_logger.h"

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
#include <cassert>
#include <filesystem>
#include <sstream>
#include <string>
#include <array>

/*
this needs to look something like, create a vertex buffer with vertex and integer indices
then bind a material to that buffer, material can be hard coded for now.
three buffer types
1) face buffer which is a list of faces which is 1:1
2) line buffer which will take in a list of indices, and lines then will generate in the gpu a set of faces for rendering, pipes
3) point buffer which will take in a list of indices, and points then will generate in the gpu a set of spheres for rendering
*/
using namespace wgpu;
using VertexAttributes = lewitt::resources::VertexAttributes;

constexpr float PI = 3.14159265358979323846f;

// Custom ImGui widgets
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

///////////////////////////////////////////////////////////////////////////////
// Public methods

bool Application::onInit()
{

	if (!initWindowAndDevice())
		return false;
	if (!initSwapChain())
		return false;
	if (!initDepthBuffer())
		return false;

	if (!initRenderables())
		return false;

	if (!initUniforms())
		return false;
	if (!initTestCompute())
		return false;
	if (!initLightingUniforms())
		return false;

	if (!initGui())
		return false;
	return true;
}

void Application::onFrame(uint frame)
{
	onCompute();

	glfwPollEvents();
	_render_scene->update();
	_render_scene->update_uniforms(m_queue);
	//_cylinder_normal_texture->get_bindings()->get_uniform_binding(_u_id)->set_member("time", static_cast<float>(glfwGetTime()));
	// m_uniforms.time = static_cast<float>(glfwGetTime());
	// m_queue.writeBuffer(m_uniformBuffer, offsetof(MyUniforms, time), &m_uniforms.time, sizeof(MyUniforms::time));
	if (frame % 200 == 0)
	{
		lewitt::logger::geometry::get_instance().clear();
		logRandLines(rand()%10000);
	}
	TextureView nextTexture = m_swapChain.getCurrentTextureView();
	if (!nextTexture)
	{
		std::cerr << "Cannot acquire next swap chain texture" << std::endl;
		return;
	}

	CommandEncoderDescriptor commandEncoderDesc;
	commandEncoderDesc.label = "Command Encoder";
	CommandEncoder encoder = m_device.createCommandEncoder(commandEncoderDesc);

	RenderPassDescriptor renderPassDesc{};

	RenderPassColorAttachment renderPassColorAttachment{};
	renderPassColorAttachment.view = nextTexture;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = LoadOp::Clear;
	renderPassColorAttachment.storeOp = StoreOp::Store;
	renderPassColorAttachment.clearValue = Color{0.05, 0.05, 0.05, 1.0};
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;

	RenderPassDepthStencilAttachment depthStencilAttachment;
	depthStencilAttachment.view = m_depthTextureView;
	depthStencilAttachment.depthClearValue = 1.0f;
	depthStencilAttachment.depthLoadOp = LoadOp::Clear;
	depthStencilAttachment.depthStoreOp = StoreOp::Store;
	depthStencilAttachment.depthReadOnly = false;
	depthStencilAttachment.stencilClearValue = 0;
#ifdef WEBGPU_BACKEND_WGPU
	depthStencilAttachment.stencilLoadOp = LoadOp::Clear;
	depthStencilAttachment.stencilStoreOp = StoreOp::Store;
#else
	depthStencilAttachment.stencilLoadOp = LoadOp::Undefined;
	depthStencilAttachment.stencilStoreOp = StoreOp::Undefined;
#endif
	depthStencilAttachment.stencilReadOnly = true;

	renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

	renderPassDesc.timestampWriteCount = 0;
	renderPassDesc.timestampWrites = nullptr;
	RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
	_render_scene->render(renderPass, m_device);
	// We add the GUI drawing commands to the render pass
	updateGui(renderPass);

	renderPass.end();
	renderPass.release();

	nextTexture.release();

	CommandBufferDescriptor cmdBufferDescriptor{};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();
	m_queue.submit(command);
	command.release();

	m_swapChain.present();

#ifdef WEBGPU_BACKEND_DAWN
	// Check for pending error callbacks
	m_device.tick();
#endif
}

void Application::onCompute()
{
	CommandEncoderDescriptor encoderDesc = Default;
	CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);

	// Create compute pass
	ComputePassDescriptor computePassDesc;
	computePassDesc.timestampWriteCount = 0;
	computePassDesc.timestampWrites = nullptr;

	ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);
	_compute_scene->compute(computePass, m_device);
	computePass.end();

	// Encode and submit the GPU commands
	CommandBuffer commands = encoder.finish(CommandBufferDescriptor{});

	m_queue.submit(commands);

#if !defined(WEBGPU_BACKEND_WGPU)
	wgpuCommandBufferRelease(commands);
	wgpuCommandEncoderRelease(encoder);
	wgpuComputePassEncoderRelease(computePass);
#endif
}

void Application::onFinish()
{
	terminateGui();
	terminateDepthBuffer();
	terminateSwapChain();
	terminateWindowAndDevice();
}

bool Application::isRunning()
{
	return !glfwWindowShouldClose(m_window);
}

void Application::onResize()
{
	// Terminate in reverse order
	terminateDepthBuffer();
	terminateSwapChain();

	// Re-init
	initSwapChain();
	initDepthBuffer();
		_render_scene->update_uniforms(m_queue);
}

void Application::onMouseMove(double xpos, double ypos)
{
		_render_scene->camera_move(xpos, ypos, m_queue);		
}

void Application::onMouseButton(int button, int action, int /* modifiers */)
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

void Application::onScroll(double  xoffset, double yoffset)
{
	_render_scene->camera_scroll(xoffset, yoffset, m_queue);
}

///////////////////////////////////////////////////////////////////////////////
// Private methods

bool Application::initWindowAndDevice()
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
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onResize(); });
	glfwSetCursorPosCallback(m_window, [](GLFWwindow *window, double xpos, double ypos)
													 {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseMove(xpos, ypos); });
	glfwSetMouseButtonCallback(m_window, [](GLFWwindow *window, int button, int action, int mods)
														 {
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onMouseButton(button, action, mods); });
	glfwSetScrollCallback(m_window, [](GLFWwindow *window, double xoffset, double yoffset)
												{
		auto that = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
		if (that != nullptr) that->onScroll(xoffset, yoffset); });

	adapter.release();
	return m_device != nullptr;
}

void Application::terminateWindowAndDevice()
{
	m_queue.release();
	m_device.release();
	m_surface.release();
	m_instance.release();

	glfwDestroyWindow(m_window);
	glfwTerminate();
}

bool Application::initSwapChain()
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

void Application::terminateSwapChain()
{
	m_swapChain.release();
}

bool Application::initDepthBuffer()
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

void Application::terminateDepthBuffer()
{
	m_depthTextureView.release();
	m_depthTexture.destroy();
	m_depthTexture.release();
}

bool Application::initTestCompute()
{
	_compute_scene = lewitt::compute_scene::create();
	// lewitt::doables::ray_compute::ptr ray_compute = lewitt::doables::ray_compute::create(m_device);
	// ray_compute->init_textures(m_device);
	//_computables.push_back(ray_compute);
	return true;
}


bool Application::initUniforms()
{
	return _render_scene->init_camera(m_window, m_device);
}

bool Application::initLightingUniforms()
{
	return _render_scene->init_lighting(m_device);
}

void Application::updateLightingUniforms()
{
	_render_scene->update_uniforms(m_queue);
}

bool Application::initBunny()
{

	auto [index_buffer, attr_buffer] = lewitt::buffers::load_bunny(m_device);
	std::cout << "creating bunny doable" << std::endl;
	lewitt::doables::renderable::ptr bunny = lewitt::doables::renderable::create(
			index_buffer, attr_buffer,
			lewitt::shaders::PN::create(m_device));

	bunny->set_texture_format(m_swapChainFormat, m_depthTextureFormat);
	_render_scene->renderables.push_back(bunny);
	return true;
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

std::vector<vec3> gen_rand_colors(int N)
{
	// use std::randomg device w/mersein twister
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis(0.0, 1.0);
	std::vector<vec3> offsets;
	for (int i = 0; i < N; i++)
	{
		offsets.push_back(vec3(dis(gen), dis(gen), dis(gen)));
	}
	return offsets;
}

// generate random unit quaternions as above, but save them as

std::vector<quat> gen_rand_quats(int N)
{
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dis(-1.0, 1.0);
	std::vector<quat> quats;
	for (int i = 0; i < N; i++)
	{
		quat q(dis(gen), dis(gen), dis(gen), dis(gen));
		q = glm::normalize(q);
		quats.push_back(q);
	}
	return quats;
}

bool Application::initBunnyInstanced()
{

	auto [index_buffer, attr_buffer] = lewitt::buffers::load_bunny(m_device);
	std::cout << "creating bunny doable" << std::endl;
	lewitt::doables::renderable::ptr _bunny = lewitt::doables::renderable::create(
			index_buffer, attr_buffer,
			lewitt::shaders::render_shader::create_from_path(RESOURCE_DIR "/pnc.wgsl", m_device));

	attr_buffer->set_vertex_layout<vec3, vec3>(wgpu::VertexStepMode::Vertex);

	lewitt::buffers::buffer::ptr offset_attr_buffer =
			lewitt::buffers::buffer::create<vec3>(gen_rand_offsets(10000), m_device,
																						lewitt::flags::vertex::read);
	offset_attr_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);
	_bunny->append_attribute_buffer(offset_attr_buffer);

	lewitt::buffers::buffer::ptr color_attr_buffer =
			lewitt::buffers::buffer::create<vec3>(gen_rand_colors(10000), m_device,
																						lewitt::flags::vertex::read);
	color_attr_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);
	_bunny->append_attribute_buffer(color_attr_buffer);

	lewitt::buffers::buffer::ptr quat_attr_buffer =
			lewitt::buffers::buffer::create<quat>(gen_rand_quats(10000), m_device,
																						lewitt::flags::vertex::read);
	quat_attr_buffer->set_vertex_layout<quat>(wgpu::VertexStepMode::Instance);
	_bunny->append_attribute_buffer(quat_attr_buffer);

	_bunny->set_instance_count(offset_attr_buffer->count());

	_bunny->set_texture_format(m_swapChainFormat, m_depthTextureFormat);
	_render_scene->renderables.push_back(_bunny);
	return true;
}

bool Application::initSphere()
{

	// auto [vertices, normals, indices] = lewitt::primitives::sphere(64, 64, 1.0,
	//																															 0.0 * M_PI, 1.0 * M_PI,
	//																															 0.5 * M_PI, 1.95 * M_PI);
	auto [vertices, normals, indices, flags] = lewitt::primitives::egg(64, 32, 0.05, 1.0, 1.2);

	lewitt::buffers::buffer::ptr attr_buffer =
			lewitt::buffers::buffer::create<vec3>(vertices, m_device, lewitt::flags::vertex::read);
	attr_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Vertex);

	lewitt::buffers::buffer::ptr norm_buffer =
			lewitt::buffers::buffer::create<vec3>(normals, m_device, lewitt::flags::vertex::read);
	norm_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Vertex);

	lewitt::buffers::buffer::ptr index_buffer =
			lewitt::buffers::buffer::create<uint32_t>(indices, m_device, lewitt::flags::index::read);

	lewitt::doables::renderable::ptr sphere = lewitt::doables::renderable::create(
			index_buffer, attr_buffer,
			lewitt::shaders::render_shader::create_from_path(RESOURCE_DIR "/pnc.wgsl", m_device));

	sphere->append_attribute_buffer(norm_buffer);

	// adding vertex layout as an option would be good, otherwise
	lewitt::buffers::buffer::ptr offset_attr_buffer =
			lewitt::buffers::buffer::create<vec3>({vec3(0.0, 0.0, 0.0)}, m_device,
																						lewitt::flags::vertex::read);
	offset_attr_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);
	sphere->append_attribute_buffer(offset_attr_buffer);

	lewitt::buffers::buffer::ptr color_attr_buffer =
			lewitt::buffers::buffer::create<vec3>({vec3(0.0, 1.0, 0.0)}, m_device,
																						lewitt::flags::vertex::read);
	color_attr_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);

	// right now need to copy the buffer, but should work, probably should work around
	// this location assignment so that we can reuse buffers
	sphere->append_attribute_buffer(color_attr_buffer);
	//_sphere->append_attribute_buffer(norm_buffer);

	lewitt::buffers::buffer::ptr quat_attr_buffer =
			lewitt::buffers::buffer::create<quat>({quat(0.0, 0.0, 0.0, 1.0)}, m_device,
																						lewitt::flags::vertex::read);
	quat_attr_buffer->set_vertex_layout<quat>(wgpu::VertexStepMode::Instance);
	sphere->append_attribute_buffer(quat_attr_buffer);

	sphere->set_instance_count(offset_attr_buffer->count());

	sphere->set_texture_format(m_swapChainFormat, m_depthTextureFormat);

	_render_scene->renderables.push_back(sphere);
	return true;
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
void Application::logRandLines(uint N)
{
	for (int i = 0; i < N; i++)
	{
		auto [v0, v1, col, r] = rand_line();
		lewitt::logger::geometry::line({v0,v1}, col, r);
	}
}

bool Application::initCapsule()
{
	std::cout << "create lines" << std::endl;
	lewitt::logger::geometry::get_instance().debugLines->set_texture_format(m_swapChainFormat, m_depthTextureFormat);
	_render_scene->renderables.push_back(lewitt::logger::geometry::get_instance().debugLines);

	logRandLines(100);
	return true;
}

bool Application::initCylinder()
{

	return true;
}

bool Application::initRenderables()
{
	_render_scene = lewitt::render_scene::create();
	initCapsule();
	return true;
}

bool Application::initGui()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::GetIO();

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOther(m_window, true);
	ImGui_ImplWGPU_Init(m_device, 3, m_swapChainFormat, m_depthTextureFormat);
	return true;
}

void Application::terminateGui()
{
	ImGui_ImplGlfw_Shutdown();
	ImGui_ImplWGPU_Shutdown();
}

void Application::updateGui(RenderPassEncoder renderPass)
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
