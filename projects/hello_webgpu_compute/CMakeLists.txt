cmake_minimum_required(VERSION 3.28)

# Get the name of the folder encapsulating the project
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

project(${PROJECT_NAME})

# Add your source files here
set(SOURCES
	Application.h
	Application.cpp
	ResourceManager.h
	ResourceManager.cpp
	implementations.cpp
	main.cpp
)

# Add your module files here
set(MODULES
  #hello.cpp
)

# Add your executable target
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} glfw webgpu glfw3webgpu imgui)
target_copy_webgpu_binaries(${PROJECT_NAME})
