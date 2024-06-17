#include <iostream>
#include "glfw_runner.hpp"

int main(int, char **)
{
  int counter = 0;
  run_glfw([](int frame, GLFWwindow *window) {
    std::cout << frame << std::endl;
  });
  return 0;
}