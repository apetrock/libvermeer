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

	if (!initTextures())
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
	updateDragInertia();
	updateLightingUniforms();
	//_cylinder_normal_texture->get_bindings()->get_uniform_binding(_u_id)->set_member("time", static_cast<float>(glfwGetTime()));
	// m_uniforms.time = static_cast<float>(glfwGetTime());
	// m_queue.writeBuffer(m_uniformBuffer, offsetof(MyUniforms, time), &m_uniforms.time, sizeof(MyUniforms::time));
	if (frame % 200 == 0)
	{
		_lines->clear();
		genRandomLines(rand()%10000, _lines);
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

	//_cylinder->draw(renderPass, m_device);
	std::for_each(_renderables.begin(), _renderables.end(), [&](const auto &e)
								{ e->draw(renderPass, m_device); });

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
	std::for_each(_computables.begin(), _computables.end(), [&](const auto &e)
								{ e->compute(computePass, m_device); });

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

	updateProjectionMatrix();
}

void Application::onMouseMove(double xpos, double ypos)
{
	if (m_drag.active)
	{
		vec2 currentMouse = vec2(-(float)xpos, (float)ypos);
		vec2 delta = (currentMouse - m_drag.startMouse) * m_drag.sensitivity;
		m_cameraState.angles = m_drag.startCameraState.angles + delta;
		// Clamp to avoid going too far when orbitting up/down
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
		updateViewMatrix();

		// Inertia
		m_drag.velocity = delta - m_drag.previousDelta;
		m_drag.previousDelta = delta;
	}
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
			m_drag.active = true;
			double xpos, ypos;
			glfwGetCursorPos(m_window, &xpos, &ypos);
			m_drag.startMouse = vec2(-(float)xpos, (float)ypos);
			m_drag.startCameraState = m_cameraState;
			break;
		case GLFW_RELEASE:
			m_drag.active = false;
			break;
		}
	}
}

void Application::onScroll(double /* xoffset */, double yoffset)
{
	m_cameraState.zoom += m_drag.scrollSensitivity * static_cast<float>(yoffset);
	m_cameraState.zoom = glm::clamp(m_cameraState.zoom, -2.0f, 2.0f);
	updateViewMatrix();
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
	requiredLimits.limits.maxVertexAttributes = 12;
	//                                          ^ This was a 4
	requiredLimits.limits.maxVertexBuffers = 10;
	requiredLimits.limits.maxBufferSize = 1500000 * sizeof(VertexAttributes);
	// requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
	requiredLimits.limits.maxVertexBufferArrayStride = 128;

	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 32;
	//                                                    ^^ This was a 11
	requiredLimits.limits.maxBindGroups = 4;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 8;
	requiredLimits.limits.maxUniformBufferBindingSize = 32 * 4 * sizeof(float);
	// Allow textures up to 2K
	requiredLimits.limits.maxTextureDimension1D = 2048;
	requiredLimits.limits.maxTextureDimension2D = 2048;
	requiredLimits.limits.maxTextureArrayLayers = 1;
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 2;
	//                                                       ^ This was 1
	requiredLimits.limits.maxSamplersPerShaderStage = 1;

	requiredLimits.limits.maxStorageBuffersPerShaderStage = 8;
	requiredLimits.limits.maxComputeWorkgroupSizeX = 256;
	requiredLimits.limits.maxComputeWorkgroupSizeY = 256;
	requiredLimits.limits.maxComputeWorkgroupSizeZ = 64;
	requiredLimits.limits.maxComputeInvocationsPerWorkgroup = 64;
	requiredLimits.limits.maxComputeWorkgroupsPerDimension = 1024;
	requiredLimits.limits.maxStorageBufferBindingSize = 1500000 * sizeof(VertexAttributes);
	;
	requiredLimits.limits.maxStorageTexturesPerShaderStage = 1;

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
	// lewitt::doables::ray_compute::ptr ray_compute = lewitt::doables::ray_compute::create(m_device);
	// ray_compute->init_textures(m_device);
	//_computables.push_back(ray_compute);
	return true;
}

bool Application::initTextures()
{
	lewitt::bindings::sampler::ptr sampler_binding =
			lewitt::bindings::default_linear_filter(ShaderStage::Fragment, m_device);
	// Create textures\
	// m_baseColorTexture = ResourceManager::loadTexture(RESOURCE_DIR "/fourareen2K_albedo.jpg", m_device, &m_baseColorTextureView);

	lewitt::bindings::texture::ptr base_texture_binding = lewitt::bindings::texture::create(
			lewitt::resources::loadTextureAndView(RESOURCE_DIR "/cobblestone_floor_08_diff_2k.jpg", m_device));

	base_texture_binding->set_frag_float_2d();
	if (!base_texture_binding->valid())
	{
		std::cerr << "Could not load base color texture!" << std::endl;
		return false;
	}

	lewitt::bindings::texture::ptr normal_texture_binding = lewitt::bindings::texture::create(
			lewitt::resources::loadTextureAndView(RESOURCE_DIR "/cobblestone_floor_08_nor_gl_2k.png", m_device));
	normal_texture_binding->set_frag_float_2d();

	// worth making specifica doables for types of shaders/properties
	_cylinder_normal_texture->get_bindings()->assign(2, base_texture_binding);
	_cylinder_normal_texture->get_bindings()->assign(3, normal_texture_binding);
	_cylinder_normal_texture->get_bindings()->assign(4, sampler_binding);

	if (!normal_texture_binding->valid())
	{
		std::cerr << "Could not load normal texture!" << std::endl;
		return false;
	}
	return true;
}

bool Application::initUniforms()
{

	using mat4 = lewitt::bindings::mat4;

	// Create uniform buffer
	std::cout << "init my uniforms" << std::endl;
	lewitt::bindings::uniform::ptr uniform_binding =
			lewitt::bindings::uniform::create<mat4, mat4, mat4, vec4, vec3, float>(
					{"projectionMatrix",
					 "viewMatrix",
					 "modelMatrix",
					 "color",
					 "cameraWorldPosition",
					 "time"},
					m_device);
	uniform_binding->set_visibility(ShaderStage::Vertex | ShaderStage::Fragment);
	uniform_binding->set_member("modelMatrix", mat4x4(1.0));
	uniform_binding->set_member("color", vec4(0.0f, 1.0f, 0.4f, 1.0f));
	uniform_binding->set_member("viewMatrix", glm::lookAt(vec3(-2.0f, -3.0f, 2.0f), vec3(0.0f), vec3(0, 0, 1)));
	uniform_binding->set_member("projectionMatrix", glm::perspective(45 * PI / 180, 640.0f / 480.0f, 0.01f, 100.0f));
	uniform_binding->set_member("time", 1.0f);
	std::cout << "scene_size: " << uniform_binding->_uniforms.size() << std::endl;
	_u_id = 0;
	std::for_each(_renderables.begin(), _renderables.end(), [&](auto &e)
								{ e->get_bindings()->assign(_u_id, uniform_binding); });

	updateProjectionMatrix();
	updateViewMatrix();
	return uniform_binding->valid();
}

bool Application::initLightingUniforms()
{
	std::cout << "init lighting" << std::endl;
	using vec4x2 = std::array<vec4, 2>;

	;
	lewitt::bindings::uniform::ptr lighting_uniform_binding =
			lewitt::bindings::uniform::create<vec4x2, vec4x2, float, float, float, float>(
					{"directions", "colors", "hardness", "kd", "ks", "pad"}, m_device);
	lighting_uniform_binding->set_visibility(ShaderStage::Fragment);

	vec4x2 dirs = {vec4(0.5f, -0.9f, 0.1f, 0.0f), vec4(0.2f, 0.4f, 0.3f, 0.0f)};
	lighting_uniform_binding->set_member("directions", dirs);
	vec4x2 colors = {vec4({1.0f, 0.9f, 0.6f, 1.0f}), vec4(0.6f, 0.9f, 1.0f, 1.0f)};
	lighting_uniform_binding->set_member("colors", colors);
	lighting_uniform_binding->set_member("hardness", 32.0f);
	lighting_uniform_binding->set_member("kd", 1.0f);
	lighting_uniform_binding->set_member("ks", 0.5f);
	lighting_uniform_binding->set_member("pad", 0.0f);
	_u_lighting_id = 1;
	std::for_each(_renderables.begin(), _renderables.end(), [&](auto &e)
								{ e->get_bindings()->assign(_u_lighting_id, lighting_uniform_binding); });

	lewitt::uniforms::test_structish();
	updateLightingUniforms();
	return lighting_uniform_binding->valid();
}

void Application::updateLightingUniforms()
{
	std::for_each(_renderables.begin(), _renderables.end(), [&](auto &e)
								{ e->get_bindings()->get_uniform_binding(_u_lighting_id)->update(m_queue); });
}

bool Application::initBunny()
{

	auto [index_buffer, attr_buffer] = lewitt::buffers::load_bunny(m_device);
	std::cout << "creating bunny doable" << std::endl;
	lewitt::doables::renderable::ptr bunny = lewitt::doables::renderable::create(
			index_buffer, attr_buffer,
			lewitt::shaders::PN::create(m_device));

	bunny->set_texture_format(m_swapChainFormat, m_depthTextureFormat);

	_renderables.push_back(bunny);
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
			lewitt::shaders::shader_t::create(RESOURCE_DIR "/pnc.wgsl", m_device));

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
	_renderables.push_back(_bunny);
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

	lewitt::doables::renderable::ptr _sphere = lewitt::doables::renderable::create(
			index_buffer, attr_buffer,
			lewitt::shaders::shader_t::create(RESOURCE_DIR "/pnc.wgsl", m_device));

	_sphere->append_attribute_buffer(norm_buffer);

	// adding vertex layout as an option would be good, otherwise
	lewitt::buffers::buffer::ptr offset_attr_buffer =
			lewitt::buffers::buffer::create<vec3>({vec3(0.0, 0.0, 0.0)}, m_device,
																						lewitt::flags::vertex::read);
	offset_attr_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);
	_sphere->append_attribute_buffer(offset_attr_buffer);

	lewitt::buffers::buffer::ptr color_attr_buffer =
			lewitt::buffers::buffer::create<vec3>({vec3(0.0, 1.0, 0.0)}, m_device,
																						lewitt::flags::vertex::read);
	color_attr_buffer->set_vertex_layout<vec3>(wgpu::VertexStepMode::Instance);

	// right now need to copy the buffer, but should work, probably should work around
	// this location assignment so that we can reuse buffers
	_sphere->append_attribute_buffer(color_attr_buffer);
	//_sphere->append_attribute_buffer(norm_buffer);

	lewitt::buffers::buffer::ptr quat_attr_buffer =
			lewitt::buffers::buffer::create<quat>({quat(0.0, 0.0, 0.0, 1.0)}, m_device,
																						lewitt::flags::vertex::read);
	quat_attr_buffer->set_vertex_layout<quat>(wgpu::VertexStepMode::Instance);
	_sphere->append_attribute_buffer(quat_attr_buffer);

	_sphere->set_instance_count(offset_attr_buffer->count());

	_sphere->set_texture_format(m_swapChainFormat, m_depthTextureFormat);
	_renderables.push_back(_sphere);

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
void Application::genRandomLines(uint N, lewitt::doables::lineable::ptr lines)
{
	for (int i = 0; i < N; i++)
	{
		auto [v0, v1, col, r] = rand_line();
		lines->add_line({v0, v1}, col, r);
	}
}

bool Application::initCapsule()
{
	std::cout << "create lines" << std::endl;
	lewitt::doables::lineable::ptr lines = lewitt::doables::lineable::create();
	std::cout << "init lines" << std::endl;
	genRandomLines(100, lines);
	_renderables.push_back(lines);
	lines->set_texture_format(m_swapChainFormat, m_depthTextureFormat);
	_lines = lines;
	return true;
}

bool Application::initCylinder()
{

	return true;
}

bool Application::initRenderables()
{

	_cylinder_normal_texture = lewitt::doables::renderable::create(
			lewitt::buffers::load_cylinder(m_device),
			lewitt::shaders::PNCUVTB::create(m_device));

	_cylinder_normal_texture->set_texture_format(m_swapChainFormat, m_depthTextureFormat);
	//_renderables.push_back(_cylinder_normal_texture);
	// bool bunny_inited = initBunnyInstanced();
	// bool sphere_inited = initSphere();
	initCapsule();
	return true;
}

void Application::updateProjectionMatrix()
{
	// Update projection matrix
	using mat4 = lewitt::bindings::mat4;

	std::cout << "update projection" << std::endl;
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	float ratio = width / (float)height;
	std::for_each(_renderables.begin(), _renderables.end(), [&](const auto &e)
								{
									lewitt::bindings::uniform::ptr uniforms = e->get_bindings()->get_uniform_binding(_u_id);
									uniforms->set_member("projectionMatrix", glm::perspective(45 * PI / 180, ratio, 0.01f, 100.0f));
									mat4 P = uniforms->get_member<mat4>("projectionMatrix");
									uniforms->update(m_queue); });
}

void Application::updateViewMatrix()
{
	std::cout << "update view matrix" << std::endl;
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);
	vec3 position = vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	std::for_each(_renderables.begin(), _renderables.end(), [&](const auto &e)
								{
									lewitt::bindings::uniform::ptr uniforms = e->get_bindings()->get_uniform_binding(_u_id);
									uniforms->set_member("viewMatrix", glm::lookAt(position, vec3(0.0f), vec3(0, 0, 1)));
									uniforms->set_member("cameraWorldPosition", position);
	uniforms->update(m_queue); });
}

void Application::updateDragInertia()
{
	constexpr float eps = 1e-4f;
	// Apply inertia only when the user released the click.
	if (!m_drag.active)
	{
		// Avoid updating the matrix when the velocity is no longer noticeable
		if (std::abs(m_drag.velocity.x) < eps && std::abs(m_drag.velocity.y) < eps)
		{
			return;
		}
		m_cameraState.angles += m_drag.velocity;
		m_cameraState.angles.y = glm::clamp(m_cameraState.angles.y, -PI / 2 + 1e-5f, PI / 2 - 1e-5f);
		// Dampen the velocity so that it decreases exponentially and stops
		// after a few frames.
		m_drag.velocity *= m_drag.intertia;
		updateViewMatrix();
	}
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
