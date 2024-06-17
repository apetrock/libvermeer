
#include <GLFW/glfw3.h>
#include <iostream>
#include <functional>
#ifndef __glfw_runner__
#define __glfw_runner__

void run_glfw(std::function<void(int, GLFWwindow *)> appLogic, int width = 640, int height = 480)
{
  if (!glfwInit())
  {
    std::cerr << "Could not initialize GLFW!" << std::endl;
    return;
  }

  GLFWwindow *window = glfwCreateWindow(width, height, "Learn WebGPU", NULL, NULL);

  if (!window)
  {
    std::cerr << "Could not open window!" << std::endl;
    glfwTerminate();
    return;
  }

  int frame = 0;
  while (!glfwWindowShouldClose(window))
  {
    appLogic(frame++, window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
}
#endif