
#pragma once

#include <functional>
#include <iostream>
#include <vector>

#include <webgpu/webgpu.hpp>

#include "lewitt/performance.hpp"
#include "lewitt/render_targets.hpp"

namespace lewitt
{
  namespace passes
  {
    inline render_targets::external_color_attachment
    swapchain_color_attachment(wgpu::SwapChain swapchain)
    {
      LEWITT_PERF_SCOPE_PATH("lewitt::passes::swapchain_color_attachment");
      render_targets::external_color_attachment color{};
      color.view = swapchain.getCurrentTextureView();
      if (!color.view)
      {
        return color;
      }
      color.state.clear_color = wgpu::Color{0.05, 0.05, 0.05, 1.0};
      color.release_view_after_pass = true;
      return color;
    }

    inline void render(wgpu::SwapChain swapchain,
                       wgpu::Device &device,
                       render_targets::frame_attachments &attachments,
                       std::function<void(wgpu::RenderPassEncoder &, wgpu::Device &)> fcn,
                       bool present = true)
    {
      LEWITT_PERF_SCOPE_PATH("lewitt::passes::render");
      render_targets::external_color_attachment color = attachments.color;
      if (!color.view && attachments.pooled_colors.empty())
      {
        color = swapchain_color_attachment(swapchain);
        if (!color.view)
        {
          std::cerr << "Cannot acquire next swap chain texture" << std::endl;
          return;
        }
      }

      wgpu::Queue queue = device.getQueue();
      wgpu::CommandEncoderDescriptor commandEncoderDesc;
      commandEncoderDesc.label = "Command Encoder";
      wgpu::CommandEncoder encoder = device.createCommandEncoder(commandEncoderDesc);

      wgpu::RenderPassDescriptor renderPassDesc{};
      renderPassDesc.setDefault();

      wgpu::RenderPassColorAttachment single_color_attachment{};
      if (!attachments.pooled_colors.empty())
      {
        renderPassDesc.colorAttachmentCount =
            static_cast<uint32_t>(attachments.pooled_colors.size());
        renderPassDesc.colorAttachments = attachments.pooled_colors.data();
      }
      else if (color.view)
      {
        single_color_attachment.view = color.view;
        single_color_attachment.resolveTarget = nullptr;
        single_color_attachment.loadOp = color.state.load_op;
        single_color_attachment.storeOp = color.state.store_op;
        single_color_attachment.clearValue = color.state.clear_color;
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &single_color_attachment;
      }

      wgpu::RenderPassDepthStencilAttachment depthStencilAttachment{};
      if (attachments.depth_view)
      {
        depthStencilAttachment.setDefault();
        depthStencilAttachment.view = attachments.depth_view;
        depthStencilAttachment.depthClearValue = attachments.depth_state.depth_clear;
        depthStencilAttachment.depthLoadOp = attachments.depth_state.load_op;
        depthStencilAttachment.depthStoreOp = attachments.depth_state.store_op;
        depthStencilAttachment.depthReadOnly = attachments.depth_state.depth_read_only;
        depthStencilAttachment.stencilClearValue = attachments.depth_state.stencil_clear;
        depthStencilAttachment.stencilLoadOp = wgpu::LoadOp::Clear;
        depthStencilAttachment.stencilStoreOp = wgpu::StoreOp::Store;
        depthStencilAttachment.stencilReadOnly = attachments.depth_state.stencil_read_only;
        renderPassDesc.depthStencilAttachment = &depthStencilAttachment;
      }

      renderPassDesc.timestampWriteCount = 0;
      renderPassDesc.timestampWrites = nullptr;
      wgpu::RenderPassEncoder renderPass = nullptr;
      {
        LEWITT_PERF_SCOPE_PATH("lewitt::passes::render::beginRenderPass");
        renderPass = encoder.beginRenderPass(renderPassDesc);
      }

      {
        LEWITT_PERF_SCOPE_PATH("lewitt::passes::render::record_callback");
        fcn(renderPass, device);
      }

      {
        LEWITT_PERF_SCOPE_PATH("lewitt::passes::render::end_pass");
        renderPass.end();
        renderPass.release();
      }

      if (color.release_view_after_pass && color.view)
      {
        color.view.release();
      }

      wgpu::CommandBufferDescriptor cmdBufferDescriptor{};
      cmdBufferDescriptor.label = "Command buffer";
      wgpu::CommandBuffer command = nullptr;
      {
        LEWITT_PERF_SCOPE_PATH("lewitt::passes::render::encoder_finish");
        command = encoder.finish(cmdBufferDescriptor);
      }
      encoder.release();
      {
        LEWITT_PERF_SCOPE_PATH("lewitt::passes::render::queue_submit");
        queue.submit(command);
      }
      command.release();

      if (present && swapchain)
      {
        LEWITT_PERF_SCOPE_PATH("lewitt::passes::render::swapchain_present");
        swapchain.present();
      }
    }

    inline void render(wgpu::SwapChain swapchain,
                       wgpu::Device &device,
                       render_targets::target_pool &target_pool,
                       render_targets::frame_attachments attachments,
                       std::function<void(wgpu::RenderPassEncoder &, wgpu::Device &)> fcn,
                       bool present = true)
    {
      attachments = render_targets::resolve_attachments(&target_pool, std::move(attachments));
      render(swapchain, device, attachments, std::move(fcn), present);
    }

    inline void compute(wgpu::Device &device,
                        std::function<void(wgpu::ComputePassEncoder &, wgpu::Device &)> compute_fcn,
                        std::function<void(wgpu::CommandEncoder &)> encoder_fcn = nullptr)
    {
      wgpu::Queue queue = device.getQueue();
      wgpu::CommandEncoderDescriptor encoderDesc = wgpu::Default;
      wgpu::CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

      wgpu::ComputePassDescriptor computePassDesc;
      computePassDesc.timestampWriteCount = 0;
      computePassDesc.timestampWrites = nullptr;

      wgpu::ComputePassEncoder compute_pass = encoder.beginComputePass(computePassDesc);
      if (compute_fcn)
        compute_fcn(compute_pass, device);
      compute_pass.end();
      if (encoder_fcn)
        encoder_fcn(encoder);
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
