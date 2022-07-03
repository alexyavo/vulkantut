#pragma once

#include "VulkanInstance.h"
#include "Window.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;

class Surface {
private:
  VkSurfaceKHR surface;
  ptr<VulkanInstance> instance;
  ptr<Window> window;

public:
  Surface(ptr<VulkanInstance> instance, ptr<Window> window): instance(instance), window(window) {
    if (glfwCreateWindowSurface(instance->get(), window->get(), nullptr, &surface) != VK_SUCCESS) {
      throw runtime_error("failed to create window surface");
    }
  }

  ~Surface() {
    vkDestroySurfaceKHR(instance->get(), surface, nullptr);
  }

public:
  VkSurfaceKHR get() { return surface; }
};

