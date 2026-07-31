#pragma once

#include <array>
#include <cmath>
#include <optional>

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
    // angles.x: yaw about world Z; angles.y: pitch
    vec2 angles = {0.8f, 0.5f};
    // Distance from look_at is exp(-zoom)
    float zoom = -1.2f;
    vec3 look_at = vec3(0.0f);
  };

  struct DragState
  {
    bool active = false;
    vec2 startMouse;
    CameraState startCameraState;

    float sensitivity = 0.01f;
    float scrollSensitivity = 0.1f;

    vec2 velocity = {0.0, 0.0};
    vec2 previousDelta;
    float intertia = 0.9f;
  };

  struct PanState
  {
    bool active = false;
    vec2 prev_mouse = vec2(0.0f);
  };

  class camera
  {
  public:
    DEFINE_CREATE_FUNC(camera);
    camera(GLFWwindow *window) : _window(window) {}

    static constexpr float k_fov_y = 45.0f * static_cast<float>(M_PI) / 180.0f;

    float aspect() const
    {
      int width = 1, height = 1;
      if (_window)
        glfwGetFramebufferSize(_window, &width, &height);
      return width / static_cast<float>(std::max(height, 1));
    }

    mat4 get_projection_matrix()
    {
      using mat4 = lewitt::bindings::mat4;
      const float dist = std::exp(-_camera_state.zoom);
      const float far_plane = std::max(100.0f, 20.0f * dist);
      return glm::perspective(k_fov_y, aspect(), 0.01f, far_plane);
    }

    // Unit direction from look_at toward the eye (orbit offset).
    vec3 offset_direction() const
    {
      const float cx = cos(_camera_state.angles.x);
      const float sx = sin(_camera_state.angles.x);
      const float cy = cos(_camera_state.angles.y);
      const float sy = sin(_camera_state.angles.y);
      return vec3(cx * cy, sx * cy, sy);
    }

    float distance() const { return std::exp(-_camera_state.zoom); }

    vec3 get_position() const
    {
      return _camera_state.look_at + offset_direction() * distance();
    }

    vec3 look_at() const { return _camera_state.look_at; }

    mat4 get_view_matrix()
    {
      return glm::lookAt(get_position(), _camera_state.look_at, vec3(0, 0, 1));
    }

    void set_framing(const vec3 &look_at, const vec2 &angles, float zoom)
    {
      _camera_state.look_at = look_at;
      _camera_state.angles = angles;
      _camera_state.angles.y =
          glm::clamp(_camera_state.angles.y, -(float)M_PI / 2.0f + 1e-5f,
                     (float)M_PI / 2.0f - 1e-5f);
      _camera_state.zoom = glm::clamp(zoom, -3.5f, 2.0f);
    }

    // Convert a unit view-offset direction (look_at → eye) to orbit angles.
    static vec2 angles_from_offset_dir(vec3 dir)
    {
      const float len = glm::length(dir);
      if (len < 1.0e-8f)
        return vec2(0.0f, 0.0f);
      dir /= len;
      const float ay =
          std::asin(glm::clamp(dir.z, -1.0f + 1e-5f, 1.0f - 1e-5f));
      const float ax = std::atan2(dir.y, dir.x);
      return vec2(ax, ay);
    }

    void update_inertia()
    {
      if (!_drag_state.active)
      {
        _camera_state.angles += _drag_state.velocity;
        _camera_state.angles.y =
            glm::clamp(_camera_state.angles.y, -(float)M_PI / 2.0f + 1e-5f,
                       (float)M_PI / 2.0f - 1e-5f);
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
      _drag_state.previousDelta = vec2(0.0f);
    }

    void move_end() { _drag_state.active = false; }

    void move(double xpos, double ypos)
    {
      vec2 currentMouse = vec2(-(float)xpos, (float)ypos);
      vec2 delta = (currentMouse - _drag_state.startMouse) * _drag_state.sensitivity;
      _camera_state.angles = _drag_state.startCameraState.angles + delta;
      _camera_state.angles.y =
          glm::clamp(_camera_state.angles.y, -(float)M_PI / 2.0f + 1e-5f,
                     (float)M_PI / 2.0f - 1e-5f);

      _drag_state.velocity = delta - _drag_state.previousDelta;
      _drag_state.previousDelta = delta;
    }

    void scroll(double /* xoffset */, double yoffset)
    {
      _camera_state.zoom +=
          _drag_state.scrollSensitivity * static_cast<float>(yoffset);
      _camera_state.zoom = glm::clamp(_camera_state.zoom, -3.5f, 2.0f);
    }

    // Focus-plane pan (star-style): slide look_at in the plane through focus,
    // normal = view axis, so the focus stays under the cursor.
    void pan_start(double xpos, double ypos)
    {
      _pan_state.active = true;
      _pan_state.prev_mouse = vec2(static_cast<float>(xpos), static_cast<float>(ypos));
    }

    void pan_end() { _pan_state.active = false; }

    void pan_move(double xpos, double ypos)
    {
      if (!_pan_state.active || !_window)
        return;

      int width = 1, height = 1;
      glfwGetFramebufferSize(_window, &width, &height);
      if (width < 1 || height < 1)
        return;

      const vec2 cur(static_cast<float>(xpos), static_cast<float>(ypos));
      const vec3 eye = get_position();
      const vec3 focus = _camera_state.look_at;
      const vec3 offset = eye - focus;
      const float dist = glm::length(offset);
      if (dist < 1.0e-8f)
        return;
      const vec3 n = offset / dist; // look_at → eye

      auto ray_hit = [&](const vec2 &mouse) -> std::optional<vec3> {
        // NDC (OpenGL-style), y flipped for GLFW top-left origin.
        const float ndc_x = 2.0f * mouse.x / static_cast<float>(width) - 1.0f;
        const float ndc_y = 1.0f - 2.0f * mouse.y / static_cast<float>(height);
        const float tan_y = std::tan(0.5f * k_fov_y);
        const float tan_x = tan_y * aspect();
        // Camera basis: forward toward focus, up ≈ world Z.
        const vec3 forward = -n; // eye → look_at
        vec3 up = vec3(0, 0, 1);
        vec3 right = glm::cross(forward, up);
        if (glm::dot(right, right) < 1.0e-10f) {
          up = vec3(0, 1, 0);
          right = glm::cross(forward, up);
        }
        right = glm::normalize(right);
        up = glm::normalize(glm::cross(right, forward));
        const vec3 dir =
            glm::normalize(forward + ndc_x * tan_x * right + ndc_y * tan_y * up);
        const float denom = glm::dot(dir, n);
        if (std::abs(denom) < 1.0e-8f)
          return std::nullopt;
        const float t = glm::dot(focus - eye, n) / denom;
        if (t < 0.0f)
          return std::nullopt;
        return eye + t * dir;
      };

      const auto prev_hit = ray_hit(_pan_state.prev_mouse);
      const auto cur_hit = ray_hit(cur);
      if (prev_hit && cur_hit)
        _camera_state.look_at += (*prev_hit - *cur_hit);
      _pan_state.prev_mouse = cur;
    }

    using pose_vec = std::array<float, 9>;

    pose_vec pose_vector() const
    {
      const vec3 p = get_position();
      const vec3 la = _camera_state.look_at;
      return pose_vec{p.x, p.y, p.z, la.x, la.y, la.z,
                      _camera_state.angles.x, _camera_state.angles.y,
                      _camera_state.zoom};
    }

    bool sample_motion(float tol = 1.0e-4f)
    {
      const pose_vec cur = pose_vector();
      if (!_pose_initialized) {
        _prev_pose = cur;
        _pose_initialized = true;
        _moving = false;
        return false;
      }
      float sum_sq = 0.0f;
      for (size_t i = 0; i < cur.size(); ++i) {
        const float d = cur[i] - _prev_pose[i];
        sum_sq += d * d;
      }
      _prev_pose = cur;
      _moving = _drag_state.active || _pan_state.active ||
                (std::sqrt(sum_sq) >= tol);
      return _moving;
    }

    bool is_moving() const { return _moving; }

    DragState _drag_state;
    PanState _pan_state;
    CameraState _camera_state;
    GLFWwindow *_window;
    pose_vec _prev_pose{};
    bool _pose_initialized = false;
    bool _moving = false;
  };
}
