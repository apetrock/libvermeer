
#pragma once

#include <webgpu/webgpu.hpp>

namespace lewitt
{
  namespace passes
  {
    inline void render(wgpu::SwapChain swapchain,
                       wgpu::Device &device,
                       wgpu::TextureView &depth_texture_view,
                       std::function<void(wgpu::RenderPassEncoder &, wgpu::Device &)> fcn)
    {
      wgpu::TextureView nextTexture = swapchain.getCurrentTextureView();
      if (!nextTexture)
      {
        std::cerr << "Cannot acquire next swap chain texture" << std::endl;
        return;
      }

      wgpu::Queue queue = device.getQueue();
      wgpu::CommandEncoderDescriptor commandEncoderDesc;
      commandEncoderDesc.label = "Command Encoder";
      wgpu::CommandEncoder encoder = device.createCommandEncoder(commandEncoderDesc);

      wgpu::RenderPassDescriptor renderPassDesc{};

      wgpu::RenderPassColorAttachment renderPassColorAttachment{};
      renderPassColorAttachment.view = nextTexture;
      renderPassColorAttachment.resolveTarget = nullptr;
      renderPassColorAttachment.loadOp = wgpu::LoadOp::Clear;
      renderPassColorAttachment.storeOp = wgpu::StoreOp::Store;
      renderPassColorAttachment.clearValue = wgpu::Color{0.05, 0.05, 0.05, 1.0};
      renderPassDesc.colorAttachmentCount = 1;
      renderPassDesc.colorAttachments = &renderPassColorAttachment;

      wgpu::RenderPassDepthStencilAttachment depthStencilAttachment;
      depthStencilAttachment.view = depth_texture_view;
      depthStencilAttachment.depthClearValue = 1.0f;
      depthStencilAttachment.depthLoadOp = wgpu::LoadOp::Clear;
      depthStencilAttachment.depthStoreOp = wgpu::StoreOp::Store;
      depthStencilAttachment.depthReadOnly = false;
      depthStencilAttachment.stencilClearValue = 0;
#ifdef WEBGPU_BACKEND_WGPU
      depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Clear;
      depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Store;
#else
      depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Undefined;
      depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Undefined;
#endif
      depthStencilAttachment.stencilReadOnly = true;

      renderPassDesc.depthStencilAttachment = &depthStencilAttachment;

      renderPassDesc.timestampWriteCount = 0;
      renderPassDesc.timestampWrites = nullptr;
      wgpu::RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

      fcn(renderPass, device);

      renderPass.end();
      renderPass.release();

      nextTexture.release();

      wgpu::CommandBufferDescriptor cmdBufferDescriptor{};
      cmdBufferDescriptor.label = "Command buffer";
      wgpu::CommandBuffer command = encoder.finish(cmdBufferDescriptor);
      encoder.release();
      queue.submit(command);
      command.release();

      swapchain.present();

#ifdef WEBGPU_BACKEND_DAWN
      // Check for pending error callbacks
      m_device.tick();
#endif
    }

    inline void compute(wgpu::Device &device,
                        std::function<void(wgpu::ComputePassEncoder &, wgpu::Device &)> compute_fcn,
                        std::function<void(wgpu::CommandEncoder &)> encoder_fcn = nullptr)
    {
      wgpu::Queue queue = device.getQueue();
      wgpu::CommandEncoderDescriptor encoderDesc = wgpu::Default;
      wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

      // Create compute pass
      wgpu::ComputePassDescriptor computePassDesc;
      computePassDesc.timestampWriteCount = 0;
      computePassDesc.timestampWrites = nullptr;

      wgpu::ComputePassEncoder compute_pass = encoder.beginComputePass(computePassDesc);
      if (compute_fcn)
        compute_fcn(compute_pass, device);
      // computePass.end();
      compute_pass.end();
      if (encoder_fcn)
        encoder_fcn(encoder);
      // Encode and submit the GPU commands
      wgpu::CommandBuffer commands = encoder.finish(wgpu::CommandBufferDescriptor{});

      queue.submit(commands);

#if !defined(WEBGPU_BACKEND_WGPU)
      wgpuCommandBufferRelease(commands);
      wgpuCommandEncoderRelease(encoder);
      wgpuComputePassEncoderRelease(compute_pass);
#endif
    }
  }
}