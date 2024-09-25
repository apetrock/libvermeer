#pragma once

#include <glfw3webgpu.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtx/polar_coordinates.hpp>

#include "bindings.hpp"
#include "common.h"
namespace lewitt
{

  struct CameraState
  {
    // angles.x is the rotation of the camera around the global vertical axis, affected by mouse.x
    // angles.y is the rotation of the camera around its local horizontal axis, affected by mouse.y
    vec2 angles = {0.8f, 0.5f};
    // zoom is the position of the camera along its local forward axis, affected by the scroll wheel
    float zoom = -1.2f;
  };

  struct DragState
  {
    // Whether a drag action is ongoing (i.e., we are between mouse press and mouse release)
    bool active = false;
    // The position of the mouse at the beginning of the drag action
    vec2 startMouse;
    // The camera state at the beginning of the drag action
    CameraState startCameraState;

    // Constant settings
    float sensitivity = 0.01f;
    float scrollSensitivity = 0.1f;

    // Inertia
    vec2 velocity = {0.0, 0.0};
    vec2 previousDelta;
    float intertia = 0.9f;
  };

  class camera
  {
  public:
    DEFINE_CREATE_FUNC(camera);
    camera(GLFWwindow *window) : _window(window) {}

    mat4 get_projection_matrix()
    {
      // Update projection matrix
      using mat4 = lewitt::bindings::mat4;

      std::cout << "update projection" << std::endl;
      int width, height;
      glfwGetFramebufferSize(_window, &width, &height);
      float ratio = width / (float)height;
      return glm::perspective(45.0f * (float)M_PI / 180.0f, ratio, 0.01f, 100.0f);
    }
    vec3 get_position()
    {
      float cx = cos(_camera_state.angles.x);
      float sx = sin(_camera_state.angles.x);
      float cy = cos(_camera_state.angles.y);
      float sy = sin(_camera_state.angles.y);
      vec3 position = vec3(cx * cy, sx * cy, sy) * std::exp(-_camera_state.zoom);
      return position;
    }
    mat4 get_view_matrix()
    {
      std::cout << "update view matrix" << std::endl;
      vec3 position = get_position();
      return glm::lookAt(position, vec3(0.0f), vec3(0, 0, 1));
    }

    void update_inertia()
    {
      constexpr float eps = 1e-4f;
      // Apply inertia only when the user released the click.
      if (!_drag_state.active)
      {
        _camera_state.angles += _drag_state.velocity;
        _camera_state.angles.y = glm::clamp(_camera_state.angles.y, -(float)M_PI / 2.0f + 1e-5f, (float)M_PI / 2.0f - 1e-5f);
        _drag_state.velocity *= _drag_state.intertia;
      }
    }

    void move_start()
    {
      _drag_state.active = true;
      double xpos, ypos;
      glfwGetCursorPos(_window, &xpos, &ypos);
      _drag_state.startMouse = vec2(-(float)xpos, (float)ypos);
      _drag_state.startCameraState = _camera_state;
    }

    void move_end()
    {
      _drag_state.active = false;
    }

    void move(double xpos, double ypos)
    {

      vec2 currentMouse = vec2(-(float)xpos, (float)ypos);
      vec2 delta = (currentMouse - _drag_state.startMouse) * _drag_state.sensitivity;
      _camera_state.angles = _drag_state.startCameraState.angles + delta;
      // Clamp to avoid going too far when orbitting up/down
//      _camera_state.angles.y = glm::clamp(_camera_state.angles.y, -M_PI / 2 + 1e-5f, M_PI / 2 - 1e-5f);
        _camera_state.angles.y = glm::clamp(_camera_state.angles.y, -(float)M_PI / 2.0f + 1e-5f, (float)M_PI / 2.0f - 1e-5f);

      // Inertia
      _drag_state.velocity = delta - _drag_state.previousDelta;
      _drag_state.previousDelta = delta;
    }

    void scroll(double /* xoffset */, double yoffset)
    {
      _camera_state.zoom += _drag_state.scrollSensitivity * static_cast<float>(yoffset);
      _camera_state.zoom = glm::clamp(_camera_state.zoom, -2.0f, 2.0f);
    }

    DragState _drag_state;
    CameraState _camera_state;
    GLFWwindow *_window;
  };
}