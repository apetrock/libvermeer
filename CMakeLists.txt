cmake_minimum_required(VERSION 3.28)
project(
  vermeer # vermeer was Dutch Baroque painter who may have used a camera obscura...
  VERSION 0.1.0 # any version number
  LANGUAGES CXX C # programming languages used by the project
)

# Set C++ standard to C++20
set(
  CMAKE_CXX_STANDARD 17
  CMAKE_CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  COMPILE_WARNING_AS_ERROR ON
)


# Add the cmake_modules directory to CMAKE_MODULE_PATH
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules")

# Build Types
set(CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE}
    CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel tsan asan lsan msan ubsan"
    FORCE)
  
include(cmake_modules/sanitizers.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodules-ts")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fsanitize=undefined")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodule-mapper=${CMAKE_CURRENT_SOURCE_DIR}/modules/vermeer_module_map.txt")


#add_subdirectory(modules)
#add_definitions(-DWEBGPU_BACKEND_WGPU=0)
#set glfw flag: _GLFW_USE_HYBRID_HPG
add_definitions(-DGLFW_USE_HYBRID_HPG=1)
add_subdirectory(ext/glfw)
add_subdirectory(ext/webgpu-native)
add_subdirectory(ext/glfw3webgpu)
add_subdirectory(ext/imgui)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include())
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext())
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/glfw/include())
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/glfw3webgpu())
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/webgpu-native/include())

set(DEV_MODE TRUE CACHE BOOL "Dev mode switch" FORCE)
if(DEV_MODE)
    add_definitions(-DRESOURCE_DIR="${CMAKE_SOURCE_DIR}/assets")
else()
    add_definitions(-DRESOURCE_DIR="./assets")
endif()

#check to see if the webgpu so is in the right place
#link_directories(${CMAKE_CURRENT_SOURCE_DIR}/ext/webgpu-native/bin/linux-x86_64)

# List of required libraries
#set(REQUIRED_LIBS glfw)

# Loop over the list, find each library, and add it to VERMEER_EXTRA_LIBS
#foreach(LIB ${REQUIRED_LIBS})
#    find_package(${LIB} REQUIRED)
#    list(APPEND VERMEER_EXTRA_LIBS ${LIB})
#endforeach()
#list(APPEND VERMEER_EXTRA_LIBS libvermeer)

#include(modules/load_modules.cmake)


#add_subdirectory(projects/hello_world)
add_subdirectory(projects/hello_webgpu_render)
add_subdirectory(projects/hello_webgpu_compute)
add_subdirectory(projects/aabb_test)
add_subdirectory(projects/lewitt)
