#pragma once

#include <vector>
#include "doables.hpp"
#include "camera.hpp"
namespace lewitt
{
  class render_scene
  {
  public:
    DEFINE_CREATE_FUNC(render_scene);
    render_scene()
    {
    }

    bool init_camera(GLFWwindow *window, wgpu::Device device)
    {
      _camera = camera::create(window);

      _camera_uniform_binding =
          lewitt::bindings::uniform::create<mat4, mat4, mat4, vec4, vec3, float>(
              {"projectionMatrix",
               "viewMatrix",
               "modelMatrix",
               "color",
               "cameraWorldPosition",
               "time"},
              device);
      _camera_uniform_binding->set_visibility(wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment);
      _camera_uniform_binding->set_member("modelMatrix", mat4(1.0));
      _camera_uniform_binding->set_member("color", vec4(0.0f, 1.0f, 0.4f, 1.0f));
      _camera_uniform_binding->set_member("viewMatrix", _camera->get_view_matrix());
      _camera_uniform_binding->set_member("projectionMatrix", _camera->get_projection_matrix());
      _camera_uniform_binding->set_member("time", 1.0f);

      std::for_each(renderables.begin(), renderables.end(), [&](auto &e)
                    { e->get_bindings()->assign(_u_camera_id, _camera_uniform_binding); });

      return _camera_uniform_binding->valid();
    }

    bool init_lighting(wgpu::Device device)
    {
      std::cout << "init lighting" << std::endl;
      using vec4x2 = std::array<vec4, 2>;

      _lighting_uniform_binding =
          lewitt::bindings::uniform::create<vec4x2, vec4x2, float, float, float, float>(
              {"directions", "colors", "hardness", "kd", "ks", "pad"}, device);
      _lighting_uniform_binding->set_visibility(wgpu::ShaderStage::Fragment);

      vec4x2 dirs = {vec4(0.5f, -0.9f, 0.1f, 0.0f), vec4(0.2f, 0.4f, 0.3f, 0.0f)};
      _lighting_uniform_binding->set_member("directions", dirs);
      vec4x2 colors = {vec4({1.0f, 0.9f, 0.6f, 1.0f}), vec4(0.6f, 0.9f, 1.0f, 1.0f)};
      _lighting_uniform_binding->set_member("colors", colors);
      _lighting_uniform_binding->set_member("hardness", 32.0f);
      _lighting_uniform_binding->set_member("kd", 1.0f);
      _lighting_uniform_binding->set_member("ks", 0.5f);
      _lighting_uniform_binding->set_member("pad", 0.0f);

      std::for_each(renderables.begin(), renderables.end(), [&](auto &e)
                    { e->get_bindings()->assign(_u_lighting_id, _lighting_uniform_binding); });

      lewitt::uniforms::test_structish();
      return _lighting_uniform_binding->valid();
    }

    void update_uniforms(wgpu::Queue queue)
    {
      _camera->update_inertia();
      _camera_uniform_binding->set_member("projectionMatrix", _camera->get_projection_matrix());
      _camera_uniform_binding->set_member("viewMatrix", _camera->get_view_matrix());
      _camera_uniform_binding->set_member("cameraWorldPosition", _camera->get_position());

      _camera_uniform_binding->update(queue);
      _lighting_uniform_binding->update(queue);
    }

    void camera_scroll(double xoffset, double yoffset, wgpu::Queue queue)
    {
      _camera->scroll(xoffset, yoffset);
      update_uniforms(queue);
    }
    void camera_move_start() { _camera->move_start(); }
    void camera_move_end() { _camera->move_end(); }
    void camera_move(double xpos, double ypos, wgpu::Queue queue)
    {
      if (_camera->_drag_state.active)
      {
        _camera->move(xpos, ypos);
        update_uniforms(queue);
      }
    }

    void update()
    {
      _camera_uniform_binding->set_member("time", static_cast<float>(glfwGetTime()));
    }

    void render(wgpu::RenderPassEncoder render_pass, wgpu::Device device)
    {
      std::for_each(renderables.begin(), renderables.end(), [&](const auto &e)
                    { e->draw(render_pass, device); });
    }

    const uint16_t _u_camera_id = 0;
    const uint16_t _u_lighting_id = 1;
    lewitt::bindings::uniform::ptr _camera_uniform_binding;
    lewitt::bindings::uniform::ptr _lighting_uniform_binding;

    camera::ptr _camera;
    std::vector<lewitt::doables::renderable::ptr> renderables;
  };

  class compute_scene
  {
  public:
    DEFINE_CREATE_FUNC(compute_scene);
    void compute(wgpu::ComputePassEncoder compute_pass, wgpu::Device device)
    {
      std::for_each(_computables.begin(), _computables.end(), [&](const auto &e)
                    { e->compute(compute_pass, device); });
    }
    std::vector<lewitt::doables::computable::ptr> _computables;
  };
}