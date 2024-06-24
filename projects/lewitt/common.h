#ifndef LEWITT_COMMON_H
#define LEWITT_COMMON_H

#include <memory>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#define DEFINE_CREATE_FUNC(classname)                                          \
  typedef std::shared_ptr<classname> ptr;                                      \
  template <typename... Args> static ptr create(Args &&...args) {              \
    return std::make_shared<classname>(std::forward<Args>(args)...);           \
  }

#define GLM_TYPEDEFS \
	using mat4 = glm::mat4x4; \
	using vec4 = glm::vec4; \
	using vec3 = glm::vec3; \
	using vec2 = glm::vec2; \
  using quat = glm::quat;\

#endif // LEWITT_COMMON_H