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
#include "ResourceManager.h"

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
using VertexAttributes = ResourceManager::VertexAttributes;

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
	if (!initTextures())
		return false;
	if (!initUniforms())
		return false;
	if (!initLightingUniforms())
		return false;
	if (!initBindGroupLayout())
		return false;
	if (!initRenderPipeline())
		return false;

	if (!initGeometry())
		return false;

	if (!initBindGroup())
		return false;
	if (!initGui())
		return false;
	return true;
}

void Application::onFrame()
{
	glfwPollEvents();
	updateDragInertia();
	updateLightingUniforms();
	_my_uniform_binding->set_member("time",static_cast<float>(glfwGetTime()));
	//m_uniforms.time = static_cast<float>(glfwGetTime());
	//m_queue.writeBuffer(m_uniformBuffer, offsetof(MyUniforms, time), &m_uniforms.time, sizeof(MyUniforms::time));

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

	renderPass.setPipeline(_foo_material->pipe_line());

	/// renderPass.setVertexBuffer(0, m_vertexBuffer, 0, m_vertexCount * sizeof(VertexAttributes));
	_geometry->set_current(renderPass);
	// renderPass.setVertexBuffer(0, _geometry->get_buffer(), 0, _geometry->size());

	// Set binding group
	renderPass.setBindGroup(0, m_bindGroup, 0, nullptr);

	renderPass.draw(_geometry->count(), 1, 0, 0);

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

void Application::onFinish()
{
	terminateGui();
	terminateBindGroup();
	terminateLightingUniforms();
	terminateUniforms();
	terminateGeometry();
	terminateTextures();
	terminateRenderPipeline();
	terminateBindGroupLayout();
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
	requiredLimits.limits.maxVertexAttributes = 6;
	//                                          ^ This was a 4
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBufferSize = 150000 * sizeof(VertexAttributes);
	requiredLimits.limits.maxVertexBufferArrayStride = sizeof(VertexAttributes);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
	requiredLimits.limits.maxInterStageShaderComponents = 17;
	//                                                    ^^ This was a 11
	requiredLimits.limits.maxBindGroups = 4;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 4;
	requiredLimits.limits.maxUniformBufferBindingSize = 32 * 4 * sizeof(float);
	// Allow textures up to 2K
	requiredLimits.limits.maxTextureDimension1D = 2048;
	requiredLimits.limits.maxTextureDimension2D = 2048;
	requiredLimits.limits.maxTextureArrayLayers = 1;
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 2;
	//                                                       ^ This was 1
	requiredLimits.limits.maxSamplersPerShaderStage = 1;

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

bool Application::initRenderPipeline()
{
	_foo_material = lewitt::geometry::foo_material::create(m_device);
	return _foo_material->initRenderPipeline(m_device, m_bindGroupLayout, m_swapChainFormat, m_depthTextureFormat);
}

void Application::terminateRenderPipeline()
{
}

bool Application::initTextures()
{
	// Create a sampler
	SamplerDescriptor samplerDesc;
	samplerDesc.addressModeU = AddressMode::Repeat;
	samplerDesc.addressModeV = AddressMode::Repeat;
	samplerDesc.addressModeW = AddressMode::Repeat;
	samplerDesc.magFilter = FilterMode::Linear;
	samplerDesc.minFilter = FilterMode::Linear;
	samplerDesc.mipmapFilter = MipmapFilterMode::Linear;
	samplerDesc.lodMinClamp = 0.0f;
	samplerDesc.lodMaxClamp = 8.0f;
	samplerDesc.compare = CompareFunction::Undefined;
	samplerDesc.maxAnisotropy = 1;
	m_sampler = m_device.createSampler(samplerDesc);

	_sampler_binding = lewitt::bindings::sampler::create(samplerDesc, m_device);
	_sampler_binding->set_type(SamplerBindingType::Filtering);
	_sampler_binding->set_visibility(ShaderStage::Fragment);

	// Create textures
	m_baseColorTexture = ResourceManager::loadTexture(RESOURCE_DIR "/cobblestone_floor_08_diff_2k.jpg", m_device, &m_baseColorTextureView);
	// m_baseColorTexture = ResourceManager::loadTexture(RESOURCE_DIR "/fourareen2K_albedo.jpg", m_device, &m_baseColorTextureView);

	_base_texture_binding = lewitt::bindings::texture::create(
			RESOURCE_DIR "/cobblestone_floor_08_diff_2k.jpg",
			m_device);
	_base_texture_binding->set_visibility(ShaderStage::Fragment);
	_base_texture_binding->set_sample_type(TextureSampleType::Float);
	_base_texture_binding->set_dimension(TextureViewDimension::_2D);

	if (!_base_texture_binding->valid())
	{
		std::cerr << "Could not load base color texture!" << std::endl;
		return false;
	}

	m_normalTexture = ResourceManager::loadTexture(RESOURCE_DIR "/cobblestone_floor_08_nor_gl_2k.png", m_device, &m_normalTextureView);
	// m_normalTexture = ResourceManager::loadTexture(RESOURCE_DIR "/fourareen2K_normals.png", m_device, &m_normalTextureView);

	_normal_texture_binding = lewitt::bindings::texture::create(
			RESOURCE_DIR "/cobblestone_floor_08_nor_gl_2k.png",
			m_device);
	_normal_texture_binding->set_visibility(ShaderStage::Fragment);
	_normal_texture_binding->set_sample_type(TextureSampleType::Float);
	_normal_texture_binding->set_dimension(TextureViewDimension::_2D);

	if (!_normal_texture_binding->valid())
	{
		std::cerr << "Could not load normal texture!" << std::endl;
		return false;
	}
	return m_baseColorTextureView != nullptr && m_normalTextureView != nullptr;
}

void Application::terminateTextures()
{
	m_baseColorTextureView.release();
	m_baseColorTexture.destroy();
	m_baseColorTexture.release();
	m_normalTextureView.release();
	m_normalTexture.destroy();
	m_normalTexture.release();
	m_sampler.release();
}

bool Application::initGeometry()
{

	_geometry = lewitt::geometry::buffer::create();
	return _geometry->init(m_device, m_queue);
}

void Application::terminateGeometry()
{

	// m_vertexBuffer.destroy();
	// m_vertexBuffer.release();
	// m_vertexCount = 0;
}

bool Application::initUniforms()
{
	using mat4 = lewitt::bindings::mat4;
	
	// Create uniform buffer
	std::cout << "init my uniforms" << std::endl;
	_my_uniform_binding =
			lewitt::bindings::uniform::create<mat4, mat4, mat4, vec4, vec3, float>(
					{"projectionMatrix", "viewMatrix", "modelMatrix", "color", "cameraWorldPosition", "time"}, m_device);
	_my_uniform_binding->set_visibility(ShaderStage::Vertex | ShaderStage::Fragment);
	_my_uniform_binding->set_member("modelMatrix", mat4x4(1.0));
	_my_uniform_binding->set_member("viewMatrix", glm::lookAt(vec3(-2.0f, -3.0f, 2.0f), vec3(0.0f), vec3(0, 0, 1)));
	_my_uniform_binding->set_member("projectionMatrix", glm::perspective(45 * PI / 180, 640.0f / 480.0f, 0.01f, 100.0f));
	_my_uniform_binding->set_member("time", 1.0f);
	_my_uniform_binding->set_member("color", vec4(0.0f, 1.0f, 0.4f, 1.0f));
	
	updateProjectionMatrix();
	updateViewMatrix();
	return _my_uniform_binding->valid();
}

void Application::terminateUniforms()
{
}

bool Application::initLightingUniforms()
{
	std::cout << "init lighting" << std::endl;
	using vec4x2 = lewitt::uniforms::vec4x2;
	_lighting_uniform_binding =
			lewitt::bindings::uniform::create<vec4x2, vec4x2, float, float, float, float>(
					{"directions", "colors", "hardness", "kd", "ks", "pad"}, m_device);
	_lighting_uniform_binding->set_visibility(ShaderStage::Fragment);

	vec4x2 dirs = {vec4(0.5f, -0.9f, 0.1f, 0.0f), vec4(0.2f, 0.4f, 0.3f, 0.0f)};
	_lighting_uniform_binding->set_member("directions", dirs);
	vec4x2 colors = {vec4({1.0f, 0.9f, 0.6f, 1.0f}), vec4(0.6f, 0.9f, 1.0f, 1.0f)};
	_lighting_uniform_binding->set_member("colors", colors);

	_lighting_uniform_binding->set_member("hardness", 32.0f);
	_lighting_uniform_binding->set_member("kd", 1.0f);
	_lighting_uniform_binding->set_member("ks", 0.5f);
	_lighting_uniform_binding->set_member("pad", 0.0f);
	
	lewitt::uniforms::test_structish();
	updateLightingUniforms();
	return _lighting_uniform_binding->valid();
}

void Application::terminateLightingUniforms()
{
}

void Application::updateLightingUniforms()
{
	_lighting_uniform_binding->update(m_queue);
	//_lighting_uniform_binding->update(m_lightingUniforms, m_queue);

}

bool Application::initBindGroupLayout()
{
	std::vector<BindGroupLayoutEntry> bindingLayoutEntries(5, Default);
	//                                                     ^ This was a 4

	// The uniform buffer binding
	
	_my_uniform_binding->set_id(0);
	_my_uniform_binding->add_to_layout(bindingLayoutEntries);
	_base_texture_binding->set_id(1);
	_base_texture_binding->add_to_layout(bindingLayoutEntries);
	_normal_texture_binding->set_id(2);
	_normal_texture_binding->add_to_layout(bindingLayoutEntries);
	_sampler_binding->set_id(3);
	_sampler_binding->add_to_layout(bindingLayoutEntries);
	_lighting_uniform_binding->set_id(4);
	_lighting_uniform_binding->add_to_layout(bindingLayoutEntries);

	// Create a bind group layout
	BindGroupLayoutDescriptor bindGroupLayoutDesc{};
	bindGroupLayoutDesc.entryCount = (uint32_t)bindingLayoutEntries.size();
	bindGroupLayoutDesc.entries = bindingLayoutEntries.data();
	m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);

	return m_bindGroupLayout != nullptr;
}

void Application::terminateBindGroupLayout()
{
	m_bindGroupLayout.release();
}

bool Application::initBindGroup()
{
	// Create a binding
	std::vector<BindGroupEntry> bindings(5);
	//                                   ^ This was a 4

	_my_uniform_binding->add_to_group(bindings);
	_base_texture_binding->add_to_group(bindings);
	_normal_texture_binding->add_to_group(bindings);
	_sampler_binding->add_to_group(bindings);
	_lighting_uniform_binding->add_to_group(bindings);

	BindGroupDescriptor bindGroupDesc;
	bindGroupDesc.layout = m_bindGroupLayout;
	bindGroupDesc.entryCount = (uint32_t)bindings.size();
	bindGroupDesc.entries = bindings.data();
	m_bindGroup = m_device.createBindGroup(bindGroupDesc);

	return m_bindGroup != nullptr;
}

void Application::terminateBindGroup()
{
	m_bindGroup.release();
}

void Application::updateProjectionMatrix()
{
	// Update projection matrix
	std::cout << "update projection" << std::endl;
	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	float ratio = width / (float)height;
	_my_uniform_binding->set_member("projectionMatrix", glm::perspective(45 * PI / 180, ratio, 0.01f, 100.0f));
	_my_uniform_binding->update(m_queue);
}

void Application::updateViewMatrix()
{
	std::cout << "update view matrix" << std::endl;
	float cx = cos(m_cameraState.angles.x);
	float sx = sin(m_cameraState.angles.x);
	float cy = cos(m_cameraState.angles.y);
	float sy = sin(m_cameraState.angles.y);
	vec3 position = vec3(cx * cy, sx * cy, sy) * std::exp(-m_cameraState.zoom);
	_my_uniform_binding->set_member("viewMatrix", glm::lookAt(position, vec3(0.0f), vec3(0, 0, 1)));
	_my_uniform_binding->set_member("cameraWorldPosition", position);
	_my_uniform_binding->update(m_queue);
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
		m_lightingUniformsChanged = changed;
	}

	// Draw the UI
	ImGui::EndFrame();
	// Convert the UI defined above into low-level drawing commands
	ImGui::Render();
	// Execute the low-level drawing commands on the WebGPU backend
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
}
