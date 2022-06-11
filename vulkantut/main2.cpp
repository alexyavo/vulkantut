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

class PhysDevice {
  VkPhysicalDevice device;

public:
  PhysDevice(VkPhysicalDevice device) : device(device) {}
  PhysDevice() : device(nullptr) {} // placate compiler... "no appropriate default constructor available"

  VkPhysicalDevice get() { return device; }

  vector<VkExtensionProperties> get_extensions() const {
    uint32_t size = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &size, nullptr);

    vector<VkExtensionProperties> extensions(size);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &size, extensions.data());

    return extensions;
  }

  bool supports_extension(const char* extension) const {
    for (auto ext : get_extensions()) {
      if (strcmp(ext.extensionName, extension) == 0) {
        return true;
      }
    }

    return false;
  }

  VkPhysicalDeviceProperties properties() const {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(device, &p);
    return p;
  }

  string name() const {
    return string(properties().deviceName);
  }

  vector<VkQueueFamilyProperties> queue_families() const {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    return queueFamilies;
  }

  bool present_support(VkSurfaceKHR surface, uint32_t queue_family_idx) const {
    VkBool32 support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, queue_family_idx, surface, &support);
    return support;
  }

  VkSurfaceCapabilitiesKHR surface_capabilities(VkSurfaceKHR surface) const {
    VkSurfaceCapabilitiesKHR result;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result);
    return result;
  }

  vector<VkSurfaceFormatKHR> surface_formats(VkSurfaceKHR surface) const {
    uint32_t size;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &size, nullptr);

    vector<VkSurfaceFormatKHR> formats(size);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &size, formats.data());

    return formats;
  }

  vector<VkPresentModeKHR> surface_present_modes(VkSurfaceKHR surface) const {
    uint32_t size;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &size, nullptr);

    vector<VkPresentModeKHR> modes(size);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &size, modes.data());
      
    return modes;
  }
};

class Window {
  GLFWwindow* glfw_window;

public:
  const int w, h;

  GLFWwindow* get() { return glfw_window; }

  bool should_close() {
    return glfwWindowShouldClose(glfw_window);
  }

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



class VulkanInstance {
private:
  const vector<const char *> validation_layers = {
    "VK_LAYER_KHRONOS_validation"
  };

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

  static uptr<VkDebugUtilsMessengerCreateInfoEXT> mk_dbg_info(PFN_vkDebugUtilsMessengerCallbackEXT dbg_cb) {
    auto res = mk_uptr<VkDebugUtilsMessengerCreateInfoEXT>();
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

private:
  uptr<VkDebugUtilsMessengerCreateInfoEXT> dbg_info;
  VkInstance instance;

public:
  VkInstance get() {
    return instance;
  }

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
        instance_info.ppEnabledLayerNames = validation_layers.data();

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

    instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instance_info.ppEnabledExtensionNames = extensions.data();
   
    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
      throw runtime_error("Failed to create instance");
    }
  }

  ~VulkanInstance() {
    vkDestroyInstance(instance, nullptr);
  }

public:
  vector<PhysDevice> find_devices(function<bool(const PhysDevice&)> pred) {
    uint32_t size = 0;
    vkEnumeratePhysicalDevices(instance, &size, nullptr);

    vector<VkPhysicalDevice> devices(size);
    vkEnumeratePhysicalDevices(instance, &size, devices.data());

    return filter(map(devices, [](VkPhysicalDevice d) { return PhysDevice(d); }), pred);
  }
};

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


class BetterTriangle {
  
public:
  BetterTriangle() {
  }
};


int main() {
  try {
    auto window = mk_ptr<Window>(800, 600);
    auto instance = mk_ptr<VulkanInstance>(true);
    auto surface = mk_ptr<Surface>(instance, window);

    auto suitable_physical_devices = instance->find_devices([surface](const PhysDevice& device) {
      if (
        device.surface_formats(surface->get()).empty() ||
        device.surface_present_modes(surface->get()).empty()
      ) {
        return false;
      }

      auto qfams = device.queue_families();
      bool graphics = false, present = false;
      for (size_t i = 0; i < qfams.size() && (!graphics || !present); ++i) {
        graphics |= qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT;
        present |= device.present_support(surface->get(), static_cast<uint32_t>(i));

        if (graphics && present) {
          break;
        }
      }

      if (!graphics || !present) {
        return false;
      }

      if (!device.supports_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
        return false;
      }

      return true;
    });

    if (suitable_physical_devices.empty()) {
      throw runtime_error("failed to find suitable GPU");
    }

    cout << format(
      "suitable physical devices: {}\n", 
      to_str(map(suitable_physical_devices, [](auto d) { return d.name(); }), "\n\t", ",\n\t")
    );

    auto physical_device = suitable_physical_devices.back();

    optional<uint32_t> graphics_family;
    optional<uint32_t> present_family;
    auto qfams = physical_device.queue_families();
    for (size_t i = 0; i < qfams.size(); ++i) {
      auto ii = static_cast<uint32_t>(i);

      if (!graphics_family.has_value() && qfams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        graphics_family = ii;
      }
      
      if (!present_family.has_value() && physical_device.present_support(surface->get(), ii)) {
        present_family = ii;
      }

      if (present_family.has_value() && graphics_family.has_value()) {
        break;
      }
    }

    if (!present_family.has_value()) {
      throw runtime_error("no presentation family found");
    }

    if (!graphics_family.has_value()) {
      throw runtime_error("no graphics family found");
    }


    while (!window->should_close()) {
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

