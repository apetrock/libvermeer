#ifndef __MONDIAN_GLM_TYPEDEFS__
#define __MONDIAN_GLM_TYPEDEFS__

// idea here is that perhaps we can add eigen typedefs as well
// I like eigen, but GLM is pretty convenient as well

#include <glm/glm.hpp>
#include <glm/ext.hpp>

using vec3 = glm::vec3;

inline float norm(const vec3 &p) { return p.length(); }
inline vec3 max(const vec3 &A, const vec3 &B)
{
  return vec3(std::max(A[0], B[0]),
              std::max(A[1], B[1]),
              std::max(A[2], B[2]));
}

inline vec3 min(const vec3 &A, const vec3 &B)
{
  return vec3(std::min(A[0], B[0]),
              std::min(A[1], B[1]),
              std::min(A[2], B[2]));
}

inline bool greater_than(const vec3 &A, const vec3 &B)
{
  return int(A[0] > B[0]) + int(A[1] > B[1]) + int(A[2] > B[2]) > 0;
};

inline bool less_than(const vec3 &A, const vec3 &B)
{
  return int(A[0] < B[0]) + int(A[1] < B[1]) + int(A[2] < B[2]) > 0;
};

inline vec3 div(const vec3 &A, const vec3 &B)
{
  return A / B;
}

#endif
