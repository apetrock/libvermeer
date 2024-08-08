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

#include "save_texture.h"

#include "stb_image.h"
#include "mondrian/aabb.hpp"

#include <glfw3webgpu/glfw3webgpu.h>

#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLFW_USE_HYBRID_HPG

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <imgui.h>
#include <backends/imgui_impl_wgpu.h>
#include <backends/imgui_impl_glfw.h>

#include <webgpu/webgpu.hpp>
#include "webgpu-release.h"

#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <array>

constexpr float PI = 3.14159265358979323846f;

using namespace wgpu;
using glm::mat4x4;
using glm::uvec2;
using glm::vec2;
using glm::vec3;
using glm::vec4;
// == Utils == //

using ray = Application::ray;

ray init_ray(uint_fast16_t x, uint_fast16_t y, uint_fast16_t width, uint_fast16_t height)
{
	ray r;
	r.origin = vec3(0.0f, 0.0f, 0.0f);
	vec2 xy = vec2(x, y) - vec2(width, height) / 2.0f;
	xy /= vec2(width, height);
	r.direction = vec3(xy[0], xy[1], 0.5f);
	//r.direction = vec3(0.0, 0.0, 1.0f);
	
	r.direction = normalize(r.direction);

	r.id = uint32_t(x) + uint32_t(y) * uint32_t(width);
	r.pad = 0;
	// x = id % width;
	// y = id / width;
	return r;
}

std::vector<ray> init_rays(uint_fast16_t width, uint_fast16_t height)
{
	// std::vector<ray> rays(width * height, ray());
	std::vector<ray> rays(width * height);
	for (uint_fast16_t y = 0; y < height; ++y)
	{
		for (uint_fast16_t x = 0; x < width; ++x)
		{
			uint32_t sz = width * height;
			ray r = init_ray(x, y, width, height);
			rays[r.id] = r;
		}
	}
	return rays;
}

// == GLFW Callbacks == //

void onWindowResize(GLFWwindow *window, int width, int height)
{
	(void)width;
	(void)height;
	auto pApp = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
	if (pApp != nullptr)
		pApp->onResize();
}

// == Application == //

bool Application::onInit()
{
	m_bufferSize = pow(2, 28) * sizeof(float);
	if (!initWindow())
		return false;
	if (!initDevice())
		return false;

	initSwapChain();
	initGui();

	initTextures();
	initBindGroupLayout();
	initComputePipeline();
	initBuffers();
	initTextureViews();
	initBindGroup();
	std::cout << "Initialization complete!" << std::endl;

	return true;
}

void Application::onFinish()
{
	terminateBindGroup();
	terminateTextureViews();
	terminateTextures();
	terminateBuffers();
	terminateComputePipeline();
	terminateBindGroupLayout();
	terminateGui();
	terminateSwapChain();
	terminateDevice();
	terminateWindow();
}

bool Application::isRunning()
{
	return !glfwWindowShouldClose(m_window);
}

bool Application::shouldCompute()
{
	return m_shouldCompute;
}

bool Application::initDevice()
{
	// Create instance
	m_instance = createInstance(InstanceDescriptor{});
	if (!m_instance)
	{
		std::cerr << "Could not initialize WebGPU!" << std::endl;
		return false;
	}

	// Create surface and adapter
	std::cout << "Requesting adapter..." << std::endl;
	m_surface = glfwGetWGPUSurface(m_instance, m_window);
	RequestAdapterOptions adapterOpts{};
	adapterOpts.compatibleSurface = nullptr;
	adapterOpts.compatibleSurface = m_surface;
	m_adapter = m_instance.requestAdapter(adapterOpts);
	std::cout << "Got adapter: " << m_adapter << std::endl;

	std::cout << "Requesting device..." << std::endl;
	SupportedLimits supportedLimits;
	m_adapter.getLimits(&supportedLimits);
	RequiredLimits requiredLimits = Default;
	requiredLimits.limits.maxVertexAttributes = 6;
	requiredLimits.limits.maxVertexBuffers = 1;
	requiredLimits.limits.maxBindGroups = 2;
	requiredLimits.limits.maxUniformBuffersPerShaderStage = 2;
	requiredLimits.limits.maxUniformBufferBindingSize = 16 * 4 * sizeof(float);
	requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;
	requiredLimits.limits.maxBufferSize = m_bufferSize;
	requiredLimits.limits.maxTextureDimension1D = 4096;
	requiredLimits.limits.maxTextureDimension2D = 4096;
	requiredLimits.limits.maxTextureDimension3D = 4096;
	requiredLimits.limits.maxTextureArrayLayers = 1;
	requiredLimits.limits.maxSampledTexturesPerShaderStage = 3;
	requiredLimits.limits.maxSamplersPerShaderStage = 1;
	requiredLimits.limits.maxVertexBufferArrayStride = 68;
	requiredLimits.limits.maxInterStageShaderComponents = 17;
	requiredLimits.limits.maxStorageBuffersPerShaderStage = 8;
	requiredLimits.limits.maxComputeWorkgroupSizeX = 256;
	requiredLimits.limits.maxComputeWorkgroupSizeY = 256;
	requiredLimits.limits.maxComputeWorkgroupSizeZ = 64;
	requiredLimits.limits.maxComputeInvocationsPerWorkgroup = 64;
	requiredLimits.limits.maxComputeWorkgroupsPerDimension = 1024;
	requiredLimits.limits.maxStorageBufferBindingSize = m_bufferSize;
	requiredLimits.limits.maxStorageTexturesPerShaderStage = 1;

	// Create device
	DeviceDescriptor deviceDesc{};
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeaturesCount = 0;
	deviceDesc.requiredLimits = &requiredLimits;
	deviceDesc.defaultQueue.label = "The default queue";
	m_device = m_adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << m_device << std::endl;

	// Add an error callback for more debug info
	m_uncapturedErrorCallback = m_device.setUncapturedErrorCallback([](ErrorType type, char const *message)
																																	{
		std::cout << "Device error: type " << type;
		if (message) std::cout << " (message: " << message << ")";
		std::cout << std::endl; 
		if( type == ErrorType::Validation)
			exit(0); });

	/*
	m_deviceLostCallback = m_device.setDeviceLostCallback([](DeviceLostReason reason, char const* message) {
		std::cout << "Device lost: reason " << reason;
		if (message) std::cout << " (message: " << message << ")";
		std::cout << std::endl;
	});
	*/

	m_queue = m_device.getQueue();

#ifdef WEBGPU_BACKEND_WGPU
	m_queue.submit(0, nullptr);
#else
	m_instance.processEvents();
#endif

	return true;
}

void Application::terminateDevice()
{
#ifndef WEBGPU_BACKEND_WGPU
	wgpuQueueRelease(m_queue);
#endif
	wgpuDeviceRelease(m_device);
	wgpuInstanceRelease(m_instance);
}

bool Application::initWindow()
{
	if (!glfwInit())
	{
		std::cerr << "Could not initialize GLFW!" << std::endl;
		return false;
	}

	// Create window
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
	m_window = glfwCreateWindow(640, 480, "Learn WebGPU", NULL, NULL);
	if (!m_window)
	{
		std::cerr << "Could not open window!" << std::endl;
		return false;
	}

	// Add window callbacks
	glfwSetWindowUserPointer(m_window, this);
	glfwSetFramebufferSizeCallback(m_window, onWindowResize);
	return true;
}

void Application::terminateWindow()
{
	glfwDestroyWindow(m_window);
	glfwTerminate();
}

void Application::initSwapChain()
{
#ifdef WEBGPU_BACKEND_DAWN
	m_swapChainFormat = TextureFormat::BGRA8Unorm;
#else
	m_swapChainFormat = m_surface.getPreferredFormat(m_adapter);
#endif

	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);

	std::cout << "Creating swapchain..." << std::endl;
	// m_swapChainDesc = {};
	m_swapChainDesc.width = (uint32_t)width;
	m_swapChainDesc.height = (uint32_t)height;
	m_swapChainDesc.usage = TextureUsage::RenderAttachment;
	m_swapChainDesc.format = m_swapChainFormat;
	m_swapChainDesc.presentMode = PresentMode::Fifo;
	m_swapChain = m_device.createSwapChain(m_surface, m_swapChainDesc);
	std::cout << "Swapchain: " << m_swapChain << std::endl;
}

void Application::terminateSwapChain()
{
	wgpuSwapChainRelease(m_swapChain);
}

void Application::initGui()
{
	// Setup Dear ImGui context
	std::cout << "Initializing Dear ImGui..." << std::endl;
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	(void)io;

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForOther(m_window, true);
	ImGui_ImplWGPU_Init(m_device, 3, m_swapChainFormat, TextureFormat::Undefined);
	std::cout << "  ...done!" << std::endl;
}

void Application::terminateGui()
{
	ImGui_ImplWGPU_Shutdown();
	ImGui_ImplGlfw_Shutdown();
}

void Application::initBuffers()
{
	std::cout << "Initializing buffers..." << std::endl;
	std::cout << "  -Uniform buffer..." << std::endl;
	BufferDescriptor desc;
	desc.label = "Uniforms";
	desc.mappedAtCreation = false;
	desc.size = sizeof(Uniforms);
	desc.usage = BufferUsage::CopyDst | BufferUsage::Uniform;
	m_uniformBuffer = m_device.createBuffer(desc);

	std::cout << "  -Ray buffer..." << std::endl;
	uint16_t width = m_inputTexture.getWidth();
	uint16_t height = m_inputTexture.getHeight();
	BufferDescriptor rays;
	rays.label = "Rays";
	rays.size = uint32_t(width * height) * sizeof(ray);
	rays.usage = BufferUsage::Storage | BufferUsage::CopyDst;
	m_rayBuffer = m_device.createBuffer(rays);
}

void Application::terminateBuffers()
{
	m_uniformBuffer.destroy();
	wgpuBufferRelease(m_uniformBuffer);
	m_rayBuffer.destroy();
	wgpuBufferRelease(m_rayBuffer);
}

void Application::initTextures()
{
	std::cout << "Initializing textures..." << std::endl;
	// Load image data

	int width, height, channels;
	uint8_t *pixelData = stbi_load(RESOURCE_DIR "/input.jpg", &width, &height, &channels, 4 /* force 4 channels */);
	if (nullptr == pixelData)
		throw std::runtime_error("Could not load input texture!");
	Extent3D textureSize = {(uint32_t)width, (uint32_t)height, 1};

	// Create texture
	TextureDescriptor textureDesc;
	textureDesc.dimension = TextureDimension::_2D;
	textureDesc.format = TextureFormat::RGBA8Unorm;
	textureDesc.size = textureSize;
	textureDesc.sampleCount = 1;
	textureDesc.viewFormatCount = 0;
	textureDesc.viewFormats = nullptr;
	textureDesc.mipLevelCount = 1;

	textureDesc.label = "Input";
	textureDesc.usage = (TextureUsage::TextureBinding | // to bind the texture in a shader
											 TextureUsage::CopyDst					// to upload the input data
	);
	m_inputTexture = m_device.createTexture(textureDesc);

	textureDesc.label = "Output";
	textureDesc.usage = (TextureUsage::TextureBinding | // to bind the texture in a shader
											 TextureUsage::StorageBinding | // to write the texture in a shader
											 TextureUsage::CopySrc					// to save the output data
	);
	m_outputTexture = m_device.createTexture(textureDesc);

	// Upload texture data for MIP level 0 to the GPU
	ImageCopyTexture destination;
	destination.texture = m_inputTexture;
	destination.origin = {0, 0, 0};
	destination.aspect = TextureAspect::All;
	destination.mipLevel = 0;
	TextureDataLayout source;
	source.offset = 0;
	source.bytesPerRow = 4 * textureSize.width;
	source.rowsPerImage = textureSize.height;

	std::cout << "Uploading input texture data..." << std::endl;
	m_queue.writeTexture(destination, pixelData, (size_t)(4 * width * height), source, textureSize);

	// Free CPU-side data
	stbi_image_free(pixelData);
}

void Application::terminateTextures()
{
	m_inputTexture.destroy();
	wgpuTextureRelease(m_inputTexture);

	m_outputTexture.destroy();
	wgpuTextureRelease(m_outputTexture);
}

void Application::initTextureViews()
{
	TextureViewDescriptor textureViewDesc;
	textureViewDesc.aspect = TextureAspect::All;
	textureViewDesc.baseArrayLayer = 0;
	textureViewDesc.arrayLayerCount = 1;
	textureViewDesc.dimension = TextureViewDimension::_2D;
	textureViewDesc.format = TextureFormat::RGBA8Unorm;
	textureViewDesc.mipLevelCount = 1;
	textureViewDesc.baseMipLevel = 0;

	textureViewDesc.label = "Input";
	m_inputTextureView = m_inputTexture.createView(textureViewDesc);

	textureViewDesc.label = "Output";
	m_outputTextureView = m_outputTexture.createView(textureViewDesc);
}

void Application::terminateTextureViews()
{
	wgpuTextureViewRelease(m_inputTextureView);
	wgpuTextureViewRelease(m_outputTextureView);
}

void Application::initBindGroup()
{
	// Create compute bind group
	std::vector<BindGroupEntry> entries(4, Default);

	// Input buffer
	entries[0].binding = 0;
	entries[0].textureView = m_inputTextureView;

	// Output buffer
	entries[1].binding = 1;
	entries[1].textureView = m_outputTextureView;

	// Uniforms
	entries[2].binding = 2;
	entries[2].buffer = m_uniformBuffer;
	entries[2].offset = 0;
	entries[2].size = sizeof(Uniforms);

	// Rays
	uint16_t width = m_inputTexture.getWidth();
	uint16_t height = m_inputTexture.getHeight();
	entries[3].binding = 3;
	entries[3].buffer = m_rayBuffer;
	entries[3].offset = 0;
	entries[3].size = uint32_t(width * height) * uint32_t(sizeof(ray));

	BindGroupDescriptor bindGroupDesc;
	bindGroupDesc.layout = m_bindGroupLayout;
	bindGroupDesc.entryCount = (uint32_t)entries.size();
	bindGroupDesc.entries = (WGPUBindGroupEntry *)entries.data();
	m_bindGroup = m_device.createBindGroup(bindGroupDesc);
}

void Application::terminateBindGroup()
{
	wgpuBindGroupRelease(m_bindGroup);
}

void Application::initBindGroupLayout()
{
	// Create bind group layout
	std::cout << "Initializing bind group layout..." << std::endl;
	std::vector<BindGroupLayoutEntry> bindings(4, Default);

	// Input image: MIP level 0 of the texture
	bindings[0].binding = 0;
	bindings[0].texture.sampleType = TextureSampleType::Float;
	bindings[0].texture.viewDimension = TextureViewDimension::_2D;
	bindings[0].visibility = ShaderStage::Compute;

	// Output image: MIP level 1 of the texture
	bindings[1].binding = 1;
	bindings[1].storageTexture.access = StorageTextureAccess::WriteOnly;
	bindings[1].storageTexture.format = TextureFormat::RGBA8Unorm;
	bindings[1].storageTexture.viewDimension = TextureViewDimension::_2D;
	bindings[1].visibility = ShaderStage::Compute;

	// Uniforms
	bindings[2].binding = 2;
	bindings[2].buffer.type = BufferBindingType::Uniform;
	bindings[2].buffer.minBindingSize = sizeof(Uniforms);
	bindings[2].visibility = ShaderStage::Compute;

	uint16_t width = m_inputTexture.getWidth();
	uint16_t height = m_inputTexture.getHeight();

	bindings[3].binding = 3;
	bindings[3].buffer.type = BufferBindingType::Storage;
	bindings[3].buffer.minBindingSize = uint32_t(width * height) * sizeof(ray);
	bindings[3].visibility = ShaderStage::Compute;

	BindGroupLayoutDescriptor bindGroupLayoutDesc;
	bindGroupLayoutDesc.entryCount = (uint32_t)bindings.size();
	bindGroupLayoutDesc.entries = bindings.data();
	m_bindGroupLayout = m_device.createBindGroupLayout(bindGroupLayoutDesc);
}

void Application::terminateBindGroupLayout()
{
	wgpuBindGroupLayoutRelease(m_bindGroupLayout);
}

void Application::initComputePipeline()
{
	std::cout << "Initializing compute pipeline..." << std::endl;
	// Load compute shader
	ShaderModule computeShaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/ray-shader.wgsl", m_device);

	// Create compute pipeline layout
	PipelineLayoutDescriptor pipelineLayoutDesc;
	pipelineLayoutDesc.bindGroupLayoutCount = 1;
	pipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout *)&m_bindGroupLayout;
	m_pipelineLayout = m_device.createPipelineLayout(pipelineLayoutDesc);

	// Create compute pipeline
	ComputePipelineDescriptor computePipelineDesc;
	computePipelineDesc.compute.constantCount = 0;
	computePipelineDesc.compute.constants = nullptr;
	computePipelineDesc.compute.entryPoint = "trace";
	computePipelineDesc.compute.module = computeShaderModule;
	computePipelineDesc.layout = m_pipelineLayout;
	m_pipeline = m_device.createComputePipeline(computePipelineDesc);
	// exit(0);
}

void Application::terminateComputePipeline()
{
	wgpuComputePipelineRelease(m_pipeline);
	wgpuPipelineLayoutRelease(m_pipelineLayout);
}

void Application::onFrame()
{
	std::cout << "Rendering frame..." << std::endl;
	glfwPollEvents();
	std::cout << "Acquiring next swap chain texture..." << std::endl;
	TextureView nextTexture = m_swapChain.getCurrentTextureView();
	if (!nextTexture)
	{
		std::cerr << "Cannot acquire next swap chain texture" << std::endl;
		return;
	}

	RenderPassDescriptor renderPassDesc = Default;
	WGPURenderPassColorAttachment renderPassColorAttachment{};
	renderPassColorAttachment.view = nextTexture;
	renderPassColorAttachment.resolveTarget = nullptr;
	renderPassColorAttachment.loadOp = WGPULoadOp_Clear;
	renderPassColorAttachment.storeOp = WGPUStoreOp_Store;
	renderPassColorAttachment.clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0};
	renderPassDesc.colorAttachmentCount = 1;
	renderPassDesc.colorAttachments = &renderPassColorAttachment;
	std::cout << "Creating command encoder..." << std::endl;
	CommandEncoder encoder = m_device.createCommandEncoder(Default);
	RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
	std::cout << "Calling onGui..." << std::endl;
	onGui(renderPass);
	renderPass.end();

	CommandBuffer command = encoder.finish(CommandBufferDescriptor{});
	std::cout << "Submitting command buffer..." << std::endl;
	m_queue.submit(command);

	std::cout << "Presenting swap chain..." << std::endl;
	m_swapChain.present();
#if !defined(WEBGPU_BACKEND_WGPU)
	wgpuCommandBufferRelease(command);
	wgpuCommandEncoderRelease(encoder);
	wgpuRenderPassEncoderRelease(renderPass);
#endif

	wgpuTextureViewRelease(nextTexture);
#ifdef WEBGPU_BACKEND_WGPU
	wgpuQueueSubmit(m_queue, 0, nullptr);
#else
	wgpuDeviceTick(m_device);
#endif
}

void Application::onGui(RenderPassEncoder renderPass)
{
	std::cout << "Rendering GUI..." << std::endl;
	ImGui_ImplWGPU_NewFrame();
	std::cout << "Calling onGui..." << std::endl;

	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

	// Display images
	{

		ImDrawList *drawList = ImGui::GetBackgroundDrawList();
		float offset = 0.0f;
		float width = 0.0f;

		// Input image
		width = m_inputTexture.getWidth() * m_settings.scale;
		drawList->AddImage((ImTextureID)m_inputTextureView, {offset, 0}, {offset + width, m_inputTexture.getHeight() * m_settings.scale});
		offset += width;
		std::cout << "offset: " << offset << std::endl;
		// Output image
		width = m_outputTexture.getWidth() * m_settings.scale;
		drawList->AddImage((ImTextureID)m_outputTextureView, {offset, 0}, {offset + width, m_outputTexture.getHeight() * m_settings.scale});
		offset += width;
		std::cout << "offset: " << offset << std::endl;
	}

	bool changed = false;
	m_uniforms.width = m_inputTexture.getWidth();
	m_uniforms.height = m_inputTexture.getHeight();
	m_uniforms.orientation = glm::quat(1.0, 0.0, 0.0, 0.0);
	m_shouldCompute = true;

	ImGui::Begin("Settings");
	ImGui::SliderFloat("Scale", &m_settings.scale, 0.0f, 2.0f);
	if (ImGui::Button("Save Output"))
	{
		std::filesystem::path path = RESOURCE_DIR "/output.png";
		saveTexture(path, m_device, m_outputTexture, 0);
	}
	ImGui::End();
	std::cout << "ImGui::Render()" << std::endl;
	ImGui::Render();
	ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPass);
	std::cout << "ImGui::Render() done" << std::endl;
}

void Application::onCompute()
{
	// Update uniforms
	m_queue.writeBuffer(m_uniformBuffer, 0, &m_uniforms, sizeof(Uniforms));

	std::vector<ray> rays = init_rays(m_inputTexture.getWidth(), m_inputTexture.getHeight());
	m_queue.writeBuffer(m_rayBuffer, 0, rays.data(), rays.size() * sizeof(ray));
	// Initialize a command encoder
	CommandEncoderDescriptor encoderDesc = Default;
	CommandEncoder encoder = m_device.createCommandEncoder(encoderDesc);

	// Create compute pass
	ComputePassDescriptor computePassDesc;
	computePassDesc.timestampWriteCount = 0;
	computePassDesc.timestampWrites = nullptr;
	ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);

	computePass.setPipeline(m_pipeline);

	for (uint32_t i = 0; i < 1; ++i)
	{
		computePass.setBindGroup(0, m_bindGroup, 0, nullptr);

		uint32_t invocationCountX = m_inputTexture.getWidth();
		uint32_t invocationCountY = m_inputTexture.getHeight();
		uint32_t workgroupSizePerDim = 8;
		// This ceils invocationCountX / workgroupSizePerDim
		uint32_t workgroupCountX = (invocationCountX + workgroupSizePerDim - 1) / workgroupSizePerDim;
		uint32_t workgroupCountY = (invocationCountY + workgroupSizePerDim - 1) / workgroupSizePerDim;

		computePass.dispatchWorkgroups(workgroupCountX, workgroupCountY, 1);
	}

	// Finalize compute pass
	computePass.end();

	// Encode and submit the GPU commands
	CommandBuffer commands = encoder.finish(CommandBufferDescriptor{});

	m_queue.submit(commands);

#if !defined(WEBGPU_BACKEND_WGPU)
	wgpuCommandBufferRelease(commands);
	wgpuCommandEncoderRelease(encoder);
	wgpuComputePassEncoderRelease(computePass);
#endif
	mondrian::aabb_build::ptr builder = mondrian::aabb_build::create();
	builder->step(0);

	m_shouldCompute = false;
}

void Application::onResize()
{
	terminateSwapChain();
	initSwapChain();
}
