#pragma once

#include <vector>

#include "vulkan_include.h"

using namespace std;

vector<VkLayerProperties> vk_get_layers() {
    uint32_t size = 0;
    vkEnumerateInstanceLayerProperties(&size, nullptr);

    vector<VkLayerProperties> layers(size);
    vkEnumerateInstanceLayerProperties(&size, layers.data());

    return layers;
}

vector<VkExtensionProperties> vk_get_extensions() {
  uint32_t size = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &size, nullptr);

  vector<VkExtensionProperties> extensions(size);
  vkEnumerateInstanceExtensionProperties(nullptr, &size, extensions.data());

  return extensions;
}

vector<const char*> glfw_required_extensions() {
  // Vulkan is a platform agnostic API, which means that you need an extension to interface with 
  // the window system. GLFW has a handy built-in function that returns the extension(s) it needs 
  // to do that which we can pass to the struct:

  /*
   *  @pointer_lifetime The returned array is allocated and freed by GLFW.  You
   *  should not free it yourself.  It is guaranteed to be valid only until the
   *  library is terminated.
   */
  uint32_t size = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&size);
  return vector<const char*>(extensions, extensions + size);
}

VkDeviceQueueCreateInfo vk_queue_create_info(u32 family_index, const float* priority) {
  VkDeviceQueueCreateInfo res{};
  res.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  res.queueFamilyIndex = family_index;
  res.queueCount = 1;
  res.pQueuePriorities = priority;
  return res;
}

