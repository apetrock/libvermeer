#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "common.h"

namespace lewitt
{
  namespace primitives
  {

    GLM_TYPEDEFS;
    inline std::vector<vec3> compute_normals(const std::vector<vec3> &vertices, const std::vector<uint32_t> &indices)
    {
      std::vector<vec3> normals(vertices.size());
      for (int i = 0; i < indices.size(); i += 3)
      {
        vec3 v0 = vertices[indices[i + 0]];
        vec3 v1 = vertices[indices[i + 1]];
        vec3 v2 = vertices[indices[i + 2]];
        vec3 normal = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        normals[indices[i + 0]] += normal;
        normals[indices[i + 1]] += normal;
        normals[indices[i + 2]] += normal;
      }
      for (int i = 0; i < normals.size(); i++)
      {
        normals[i] = glm::normalize(normals[i]);
      }

      return std::move(normals);
    }

    inline std::tuple<
        std::vector<vec3>,    // positions
        std::vector<vec3>,    // normals
        std::vector<uint32_t> // indices
        >
    sphere(uint16_t N_long, uint16_t N_lat, float radius,
           float phi0 = 0, float phi1 = M_PI,
           float theta0 = 0, float theta1 = 2.0 * M_PI)
    {
      // first we figure out the whole sphere, then we figure out the half sphere

      std::vector<vec3> vertices;
      std::vector<uint32_t> indices;
      auto add_vertex = [&](float radius, float theta, float phi)
      {
        vertices.push_back(vec3(
            radius * sin(phi) * cos(theta),
            radius * sin(phi) * sin(theta),
            radius * cos(phi)));
      };

      float d_phi = phi1 - phi0;
      float d_theta = theta1 - theta0;
      for (uint16_t j = 1; j < N_long; j++)
      {
        for (uint16_t i = 0; i < N_lat; i++)
        {
          float theta = theta0 + i * d_theta / N_lat;
          float phi = phi0 + (float(j) + 0.5) * d_phi / (N_long + 1);
          // std::cout << i << " " << j << " " << phi << " " << theta <<  std::endl;
          add_vertex(radius, theta, phi);
        }
      }
      int N_long_seg = d_phi < M_PI ? N_long - 1 : N_long;
      int N_lat_seg = d_theta < 2.0 * M_PI ? N_lat - 1 : N_lat;

      for (uint16_t j = 1; j < N_long - 1; j++)
      {
        for (uint16_t i = 0; i < N_lat_seg; i++)
        {

          int i0 = (j - 1) * N_lat + i;
          int i1 = (j - 1) * N_lat + (i + 1) % N_lat;
          int i2 = (j + 0) * N_lat + i;
          int i3 = (j + 0) * N_lat + (i + 1) % N_lat;
          indices.push_back(i0);
          indices.push_back(i3);
          indices.push_back(i1);
          indices.push_back(i0);
          indices.push_back(i2);
          indices.push_back(i3);
        }
      }
      int N = N_lat;
      if (phi0 < 1e-16)
      {
        int top_index = vertices.size();

        add_vertex(radius, 0, 0);

        N = theta0 > 0 ? N_lat - 1 : N_lat;
        N = theta1 < 2.0 * M_PI ? N_lat - 1 : N_lat;

        for (int i = 0; i < N; i++)
        {
          int i0 = top_index;
          int i1 = i;
          int i2 = (i + 1) % N_lat;
          indices.push_back(i0);
          indices.push_back(i1);
          indices.push_back(i2);
        }
      }
      if (phi1 >= M_PI)
      {
        int bottom_index = vertices.size();
        add_vertex(radius, 0, M_PI);
        N = theta0 > 0 ? N_lat - 1 : N_lat;
        N = theta1 < 2.0 * M_PI ? N_lat - 1 : N_lat;
        for (int i = 0; i < N; i++)
        {
          int i0 = bottom_index;
          int i1 = (N_long - 2) * N_lat + (i + 1) % N_lat;
          int i2 = (N_long - 2) * N_lat + i;
          indices.push_back(i0);
          indices.push_back(i1);
          indices.push_back(i2);
        }
      }

      std::vector<vec3> normals = compute_normals(vertices, indices);

      return {vertices, normals, indices};
    }

    std::tuple<
        std::vector<vec3>,    // positions
        std::vector<vec3>,    // normals
        std::vector<uint32_t> // indices
        > inline cylinder(uint16_t N_long, float radius, bool capped = false)
    // if its capped, then we add a few more faces for the caps
    {

      std::vector<vec3> vertices;
      std::vector<vec3> normals;
      std::vector<uint32_t> indices;
    }

    inline std::tuple<
        std::vector<vec3>,     // positions
        std::vector<vec3>,     // normals
        std::vector<uint32_t>, // indices
        std::vector<uint32_t>  // flags
        >
    egg(uint16_t N_long, uint16_t N_lat, float r0, float r1, float d)
    {
      float dp = sqrt(pow(d, 2.0) - pow(r1 - r0, 2.0));
      float theta = M_PI / 2 - atan2(fabs(r0 - r1), dp);
      float theta2 = M_PI / 2 - atan(fabs(r0 - r1) / dp);

      auto [vertices0, normals0, indices0] = sphere(N_long / 2, N_lat, r0, 0.0, theta);
      auto [vertices1, normals1, indices1] = sphere(N_long / 2, N_lat, r1, theta, M_PI);

      int N_half = vertices0.size();
      std::transform(vertices0.begin(), vertices0.end(), vertices0.begin(), [&](const vec3 &v)
                     { return vec3(v[0], v[1], v[2] + d / 2); });
      std::transform(vertices1.begin(), vertices1.end(), vertices1.begin(), [&](const vec3 &v)
                     { return vec3(v[0], v[1], v[2] - d / 2); });
      std::transform(indices1.begin(), indices1.end(), indices1.begin(), [&](const int &idx)
                     { return idx + N_half; });

      vertices0.reserve(vertices0.size() + vertices1.size());
      normals0.reserve(normals0.size() + normals1.size());
      indices0.reserve(indices0.size() + indices1.size());
      std::copy(vertices1.begin(), vertices1.end(), std::back_inserter(vertices0));
      std::copy(normals1.begin(), normals1.end(), std::back_inserter(normals0));
      std::copy(indices1.begin(), indices1.end(), std::back_inserter(indices0));

      for (int i = 0; i < N_lat; i++)
      {
        int i0 = N_half - N_lat - 1 + i;
        int i1 = N_half - N_lat - 1 + (i + 1) % N_lat;
        int i2 = N_half + i;
        int i3 = N_half + (i + 1) % N_lat;
        indices0.push_back(i0);
        indices0.push_back(i3);
        indices0.push_back(i1);
        indices0.push_back(i0);
        indices0.push_back(i2);
        indices0.push_back(i3);
      }

      std::vector<uint32_t> flags(indices0.size(), 0);
      std::transform(flags.begin(), flags.begin() + N_half, flags.begin(), [&](const int &idx)
                     { return 1; });

      return {vertices0, normals0, indices0, flags};
    }

    inline std::tuple<
        std::vector<vec3>,     // positions
        std::vector<vec3>,     // normals
        std::vector<uint32_t>, // start cap, middle, end cap flags per face
        std::vector<uint32_t>  // indices
        >
    capsule(uint16_t N_long, uint16_t N_lat, float radius, float height)
    // Nc is the number of latitudinal segments around the cylinder
    // Ns is the number of longitudinal segments around the sphere

    {

      std::vector<vec3> vertices;
      std::vector<vec3> normals;
      std::vector<uint32_t> indices;

      // basically it we need to encode a sphere and an uncapped cylinder... so,
      // make primitives for those...
      (void)N_long;
      (void)N_lat;
      (void)radius;
      (void)height;
      return {vertices, normals, {}, indices};
    }

    /// Canonical ring torus for instancing: major=1, minor=0.25, axis +Z.
    /// One full turn in u and v.
    /// Instance shader: local = major * radial + minor * tube_hat.
    inline std::tuple<std::vector<vec3>, std::vector<vec3>, std::vector<uint32_t>>
    torus(uint16_t N_major = 48, uint16_t N_minor = 24) {
      std::vector<vec3> vertices;
      std::vector<vec3> normals;
      std::vector<uint32_t> indices;
      vertices.reserve(static_cast<size_t>(N_major) * N_minor);
      normals.reserve(static_cast<size_t>(N_major) * N_minor);

      constexpr float kMajor = 1.0f;
      constexpr float kMinor = 0.25f;
      for (uint16_t i = 0; i < N_major; ++i) {
        const float u = 2.0f * static_cast<float>(M_PI) * float(i) / float(N_major);
        const float cu = std::cos(u);
        const float su = std::sin(u);
        for (uint16_t j = 0; j < N_minor; ++j) {
          const float v =
              2.0f * static_cast<float>(M_PI) * float(j) / float(N_minor);
          const float cv = std::cos(v);
          const float sv = std::sin(v);
          const vec3 radial(cu, su, 0.0f);
          const vec3 tube = cv * radial + sv * vec3(0.0f, 0.0f, 1.0f);
          const vec3 p =
              (kMajor + kMinor * cv) * radial + kMinor * sv * vec3(0.0f, 0.0f, 1.0f);
          vertices.push_back(p);
          normals.push_back(glm::normalize(tube));
        }
      }

      for (uint16_t i = 0; i < N_major; ++i) {
        const uint16_t i1 = (i + 1) % N_major;
        for (uint16_t j = 0; j < N_minor; ++j) {
          const uint16_t j1 = (j + 1) % N_minor;
          const uint32_t a = uint32_t(i) * N_minor + j;
          const uint32_t b = uint32_t(i1) * N_minor + j;
          const uint32_t c = uint32_t(i1) * N_minor + j1;
          const uint32_t d = uint32_t(i) * N_minor + j1;
          indices.push_back(a);
          indices.push_back(b);
          indices.push_back(c);
          indices.push_back(a);
          indices.push_back(c);
          indices.push_back(d);
        }
      }
      return {vertices, normals, indices};
    }
  }
}