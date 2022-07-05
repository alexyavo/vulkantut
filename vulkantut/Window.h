#pragma once

#include <iostream>

#include "vulkan_include.h"
#include "utils.h"

using namespace std;
using namespace utils;


class Window {
  GLFWwindow* glfw_window;
  bool resized = false;

  static void resize_cb(GLFWwindow* win, int w, int h) {
    cout << format("... resize event: {}x{}\n", w, h);
    auto this_win = reinterpret_cast<Window*>(glfwGetWindowUserPointer(win));
    this_win->resized = true;
  }

public:
  int w, h;

  GLFWwindow* get() { return glfw_window; }

  bool should_close() {
    return glfwWindowShouldClose(glfw_window);
  }

  Window(int w, int h) : w(w), h(h) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfw_window = glfwCreateWindow(w, h, "Vulkan", nullptr, nullptr);
    glfwSetWindowUserPointer(glfw_window, this);
    glfwSetFramebufferSizeCallback(glfw_window, resize_cb); // TODO can't use lambda here ? must have static func?
  }

  ~Window() {
    cout << "~Window\n";
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
  }

  bool check_resize() {
    auto res = resized;
    resized = false;
    return res;
  }

  void wait_minimized() {
    glfwGetFramebufferSize(glfw_window, &w, &h);
    while (w == 0 || h == 0) {
      glfwGetFramebufferSize(glfw_window, &w, &h);
      glfwWaitEvents();
    }
  }
};
