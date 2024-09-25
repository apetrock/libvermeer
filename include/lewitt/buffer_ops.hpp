#pragma once
#include <stack>
#include "buffers.hpp"
#include "bindings.hpp"
#include "shaders.hpp"
// there will have to be scene uniforms and buffer uniforms,
// I think we can seperate all of those out.
namespace lewitt
{
  GLM_TYPEDEFS;


  namespace buffers
  {

    std::string op_shader(
        const std::string &TYPE,
        const std::string &OP,
        const std::string &NAME)
    {

      std::string shader = R"(
    @group(0) @binding(0) var<storage,read> A: array<##TYPE##>;
    @group(0) @binding(1) var<storage,read> B: array<##TYPE##>;
    @group(0) @binding(2) var<storage,read_write> C: array<##TYPE##>;

    @compute @workgroup_size(32, 1, 1)
    fn ##NAME##(@builtin(global_invocation_id) id: vec3<u32>) {
        C[id.x] = A[id.x] ##OP## B[id.x];
        //C[id.x] = 1.0;
    }
    )";

      // Replace placeholders with actual values
      size_t pos;
      while ((pos = shader.find("##TYPE##")) != std::string::npos)
        shader.replace(pos, 8, TYPE);
      while ((pos = shader.find("##OP##")) != std::string::npos)
        shader.replace(pos, 6, OP);
      while ((pos = shader.find("##NAME##")) != std::string::npos)
        shader.replace(pos, 8, NAME);

      return shader;
    }

    inline buffer::ptr op(
        const std::string &TYPE,
        const std::string &OP,
        const std::string &NAME,
        const buffer::ptr &A,
        const buffer::ptr &B,
        wgpu::Device &device)
    {
      assert(A->size() == B->size());
      std::string shader_src = op_shader(TYPE, OP, NAME);
      buffer::ptr out = buffer::create(A->count(), A->format_size(), device, flags::storage::write);
      bindings::group::ptr bindings = bindings::group::create();
      std::cout << "a bind" << std::endl;

      bindings::buffer::ptr A_bind = bindings::buffer::create(A);
      A_bind->set_visibility(wgpu::ShaderStage::Compute);
      A_bind->set_binding_type(wgpu::BufferBindingType::ReadOnlyStorage);
      std::cout << "b bind" << std::endl;

      bindings::buffer::ptr B_bind = bindings::buffer::create(B);
      B_bind->set_visibility(wgpu::ShaderStage::Compute);
      B_bind->set_binding_type(wgpu::BufferBindingType::ReadOnlyStorage);

      std::cout << "c bind" << std::endl;
      bindings::buffer::ptr out_bind = bindings::buffer::create(out);
      out_bind->set_visibility(wgpu::ShaderStage::Compute);
      out_bind->set_binding_type(wgpu::BufferBindingType::Storage);

      bindings->assign(0, A_bind);
      bindings->assign(1, B_bind);
      bindings->assign(2, out_bind);
      std::cout << "init layout" << std::endl;
      bool layout_successful = bindings->init_layout(device);
      std::cout << "layout successful: " << layout_successful << std::endl;
      std::cout << "init" << std::endl;
      bindings->init(device);
      // we'll create a compute context which will store the invocation count and a map of pipelines which can be
      // lazily initialized.
      shaders::compute_shader::ptr compute_shader = shaders::compute_shader::create_from_src(shader_src, NAME, device);
      compute_shader->init(device, bindings->get_layout());

      buffer::ptr map_buff = buffer::create(out->count(), out->format_size(), device, flags::storage::map);

      std::cout << "compute" << std::endl;

      passes::compute(device, [&](wgpu::ComputePassEncoder &compute_pass, wgpu::Device &device)
                      {
                        compute_pass.setPipeline(compute_shader->compute_pipe_line());
                        for (uint32_t i = 0; i < 1; ++i)
                        {
                          compute_pass.setBindGroup(0, bindings->get_group(), 0, nullptr);
                          
                          uint32_t invocationCount = A->size() / sizeof(float);
                          uint32_t workgroupSize = 32;
                          // This ceils invocationCount / workgroupSize
                          uint32_t workgroupCount = (invocationCount + workgroupSize - 1) / workgroupSize;
                          compute_pass.dispatchWorkgroups(workgroupCount, 1, 1);
                        } },[&](wgpu::CommandEncoder & encoder){
                          	encoder.copyBufferToBuffer(out->get_buffer(), 0, map_buff->get_buffer(), 0, out->size());

                        }
                        );
      std::cout << "done!" << std::endl;
      bool done = false;
      auto handle = map_buff->get_buffer().mapAsync(wgpu::MapMode::Read, 0, map_buff->size(), [&](wgpu::BufferMapAsyncStatus status) {
        if (status == wgpu::BufferMapAsyncStatus::Success) {
          const vec3* output = (const vec3*)map_buff->get_buffer().getConstMappedRange(0, map_buff->size());
          for (int i = 0; i < map_buff->count(); ++i) {
            std::cout << "out[0]["<<i<<"]: " << output[i][0] << " ";
          }
          std::cout << std::endl;
          for (int i = 0; i < map_buff->count(); ++i) {
            std::cout << "out[1]["<<i<<"]: " << output[i][1] << " ";
          }
          std::cout << std::endl;
          for (int i = 0; i < map_buff->count(); ++i) {
            std::cout << "out[2]["<<i<<"]: " << output[i][2] << " ";
          }
          std::cout << std::endl;

          map_buff->get_buffer().unmap();
        }
        done = true;
      });
      while (!done) {
        device.getQueue().submit(0, nullptr);
      }
      return out;
    }

    inline buffer::ptr add_f32(buffer::ptr A, buffer::ptr B, wgpu::Device &device)
    {
      return op("f32", "+", "add_f32", A, B, device);
    }
    inline buffer::ptr add_vec3f(buffer::ptr A, buffer::ptr B, wgpu::Device &device)
    {
      return op("vec3f", "+", "add_vec3f", A, B, device);
    }
    inline buffer::ptr add_vec4f(buffer::ptr A, buffer::ptr B, wgpu::Device &device)
    {
      return op("vec4f", "+", "add_vec4f", A, B, device);
    }
  }
}