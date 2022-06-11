#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp


#include <stdint.h> // numeric_limits<uint32_t>::max() doesn't work, use UINT_MAX instead

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <optional>
#include <set>
#include <fstream>
#include <format>
#include <functional>
#include <algorithm>
#include <sstream>

#include "utils.h"

using namespace std;
using namespace utils;

class Window {
public:
  GLFWwindow* glfw_window;
  const int w, h;

  Window(int w, int h): w(w), h(h) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); 
    glfw_window = glfwCreateWindow(w, h, "Vulkan", nullptr, nullptr);
  }

  ~Window() {
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
  }
};

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


static VKAPI_ATTR VkBool32 VKAPI_CALL
dbg_cb(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData
) {
  if (
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
    ) {
    cout << format("[vkdbgcb] {}\n", pCallbackData->pMessage);
  }

  return VK_FALSE;
}


class VulkanInstance {
private:
  const vector<const char *> validation_layers = {
    "VK_LAYER_KHRONOS_validation"
  };

  VkInstance instance;

  static VkApplicationInfo mk_app_info() {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
    return app_info;
  }

  static unique_ptr<VkDebugUtilsMessengerCreateInfoEXT> mk_dbg_info(PFN_vkDebugUtilsMessengerCallbackEXT dbg_cb) {
    auto res = make_unique<VkDebugUtilsMessengerCreateInfoEXT>();
    *res = {};

    res->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    res->messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    res->messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    res->pfnUserCallback = dbg_cb;
    res->pUserData = nullptr; // Optional

    return res;
  }

  unique_ptr<VkDebugUtilsMessengerCreateInfoEXT> dbg_info = nullptr;

public:
  VulkanInstance(bool debug = false) {
    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    auto app_info = mk_app_info();
    instance_info.pApplicationInfo = &app_info;

    vector<const char*> extensions = glfw_required_extensions();

    if (debug) {
      // validation layers
      {
        auto vk_layers_names = to_set(map(vk_get_layers(), [](auto layer) { return string(layer.layerName); }));
        auto validation_layers_names = to_set(map(validation_layers, [](auto vl) { return string(vl); }));
        if (!is_subset_of(validation_layers_names, vk_layers_names)) {
          throw runtime_error(format(
            "Required validation layers {} could not be found among available ones: {}",
            to_str(validation_layers_names),
            to_str(vk_layers_names)
          ));
        }

        cout << format("validation layers: \n{}\n", to_str(validation_layers_names, "\t", ",\n\t", ""));

        instance_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        instance_info.ppEnabledExtensionNames = validation_layers.data();

        dbg_info = mk_dbg_info(dbg_cb);
        instance_info.pNext = dbg_info.get();
      }

      // extensions
      {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        auto extensions_names = to_set(map(extensions, [](auto e) { return string(e); }));
        auto vk_extensions_names = to_set(map(vk_get_extensions(), [](auto ext) { return string(ext.extensionName); }));
        if (!is_subset_of(extensions_names, vk_extensions_names)) {
          throw runtime_error(format("Required extensions {} could be found among available ones: {}",
            to_str(extensions_names),
            to_str(vk_extensions_names)
          ));
        }

        cout << format("extensions: \n{}\n", to_str(extensions_names, "\t", ",\n\t", ""));
      }
    }

    instance_info.enabledExtensionCount = extensions.size();
    instance_info.ppEnabledExtensionNames = extensions.data();
    
    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
      throw runtime_error("Failed to create instance");
    }
  }

  ~VulkanInstance() {
    vkDestroyInstance(instance, nullptr);
  }
};

class BetterTriangle {
  
public:
  BetterTriangle() {
  }
};


int main() {
  try {
    Window w(800, 600);
    VulkanInstance vkInstance(true);

    while (!glfwWindowShouldClose(w.glfw_window)) {
      glfwPollEvents();
      //drawFrame();
    }

    //vkDeviceWaitIdle(ldevice);
  } catch (const exception& ex) {
    cerr << ex.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

