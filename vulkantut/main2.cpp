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
    //severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
    ) {
    cout << format("[vkdbgcb] {}\n", pCallbackData->pMessage);
  }

  return VK_FALSE;
}


/*
 * VkDeviceQueueCreateInfo 
 *
 * queueFamilyIndex is an unsigned integer indicating the index of the queue
 * family in which to create the queues on this device. This index corresponds
 * to the index of an element of the pQueueFamilyProperties array that was
 * returned by vkGetPhysicalDeviceQueueFamilyProperties.
 * 
 * So the flow is: 
 * - get a physical device
 * - get queue families
 * - each queue has an index associated w/ it 
 * - index needed to check "present support" on a specific surface
 * - to create logical device you need to declare queue infos, you do this by passing the index of the queues
 * - after creating the logical device you get device queue by index
 */
class QueueFamily {
public:
  const VkQueueFamilyProperties properties;
  const uint32_t index;

  QueueFamily(VkQueueFamilyProperties properties, uint32_t index) : properties(properties), index(index) {}

  bool supports_graphics() const {
    return properties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
  }
};

class PhysDevice {
  VkPhysicalDevice device;

public:
  PhysDevice(VkPhysicalDevice device) : device(device) {}

  VkPhysicalDevice get() { return device; }

  vector<VkExtensionProperties> get_extensions() const {
    uint32_t size = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &size, nullptr);

    vector<VkExtensionProperties> extensions(size);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &size, extensions.data());

    return extensions;
  }

  bool supports_extension(const char* extension) const {
    for (auto& ext : get_extensions()) {
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

  vector<QueueFamily> queue_families() const {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    vector<VkQueueFamilyProperties> properties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, properties.data());

    return map_enumerated(
      properties, 
      [](u32 index, VkQueueFamilyProperties props) { return QueueFamily(props, index); }
    );
  }

  vector<QueueFamily> graphics_queue_families() const {
    return filter(
      queue_families(),
      [](const QueueFamily& qfam) { return qfam.supports_graphics(); }
    );
  }

  vector<QueueFamily> present_queue_families(VkSurfaceKHR surface) const {
    return filter(
      queue_families(),
      [this, surface](const QueueFamily& qfam) { return present_support(surface, qfam.index); }
    );
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
  // should this return pointers? PhysDevice is a wrapper around a vulkan handle (VkPhysicalDevice) 
  // so it works this way but...  
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

VkDeviceQueueCreateInfo vk_queue_create_info(u32 family_index, const float* priority) {
  VkDeviceQueueCreateInfo res{};
  res.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  res.queueFamilyIndex = family_index;
  res.queueCount = 1;
  res.pQueuePriorities = priority;
  return res;
}

class LogicalDevice {
  VkDevice device;
  VkQueue graphics_q;
  VkQueue present_q;

public:
  ~LogicalDevice() {
    vkDestroyDevice(device, nullptr);
  }

  LogicalDevice(
    VkPhysicalDevice physical_device, 
    const QueueFamily& graphics_queue_family,
    const QueueFamily& present_queue_family
  ) {
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    float default_q_priority = 1.0f;
    // the use of to_vector here is because we need a vector result, and result type is determined based on
    // the type of input collection. 
    // but... internally map() also uses push_back which isn't available for set()... 
    vector<VkDeviceQueueCreateInfo> queue_create_infos = map(
      to_vector(set<u32> { graphics_queue_family.index, present_queue_family.index }),
      [&default_q_priority](u32 qfam_index) { return vk_queue_create_info(qfam_index, &default_q_priority); }
    );

    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.queueCreateInfoCount = queue_create_infos.size();

    VkPhysicalDeviceFeatures features{};
    create_info.pEnabledFeatures = &features;

    // TODO hardcoded 
    vector<const char*> device_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    create_info.enabledExtensionCount = device_extensions.size();
    create_info.ppEnabledExtensionNames = device_extensions.data();

    // TODO hardcoded
    vector<const char*> validation_layers = {
      "VK_LAYER_KHRONOS_validation"
    };
    create_info.enabledLayerCount = validation_layers.size();
    create_info.ppEnabledLayerNames = validation_layers.data();

    if (vkCreateDevice(physical_device, &create_info, nullptr, &device) != VK_SUCCESS) {
      throw runtime_error("failed to create logical device");
    }

    vkGetDeviceQueue(device, graphics_queue_family.index, 0, &graphics_q);
    vkGetDeviceQueue(device, present_queue_family.index, 0, &present_q);
  }
};

class BetterTriangle {
public:
  ptr<Window> window;
  ptr<VulkanInstance> instance;
  ptr<Surface> surface; 

  // this devision of pointers and non-pointers... ugh
  // depends on your understanding of whether the object has RAII associated with it
  // or if, like PhysDevice, it's just a wrapper around a Vulkan handle (ptr) that 
  // wraps it in a nicer interface
  ptr<PhysDevice> physical_device; 

  ptr<LogicalDevice> device;

  static ptr<PhysDevice> find_physical_device(ptr<VulkanInstance> instance, ptr<Surface> surface) {
    auto suitable_physical_devices = instance->find_devices([surface](const PhysDevice& device) {
      return 
        !device.surface_formats(surface->get()).empty() &&
        !device.surface_present_modes(surface->get()).empty() &&
        !device.graphics_queue_families().empty() &&
        !device.present_queue_families(surface->get()).empty() &&
        device.supports_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
      }
    );

    if (suitable_physical_devices.empty()) {
      throw runtime_error("failed to find suitable GPU");
    }

    cout << format(
      "suitable physical devices: {}\n",
      to_str(map(suitable_physical_devices, [](auto d) { return d.name(); }), "\n\t", ",\n\t")
    );

    // should be ok? The vulkan handle is copied over...
    return mk_ptr<PhysDevice>(suitable_physical_devices.back());
  }
  
public:
  BetterTriangle(uint32_t height, uint32_t width) {
    window = mk_ptr<Window>(height, width);
    instance = mk_ptr<VulkanInstance>(true);
    surface = mk_ptr<Surface>(instance, window);

    physical_device = find_physical_device(instance, surface);
    device = mk_ptr<LogicalDevice>(
      physical_device->get(), 
      physical_device->graphics_queue_families().back(),
      physical_device->present_queue_families(surface->get()).back()
    );

    cout << "logical device created!!\n";
  }
};


int main() {
  try {
    auto triangle = mk_ptr<BetterTriangle>(800, 600);

    /*
    * 
    * to create logical device:
    * - need physical device 
    * - validation layers 
    * - need queue create info, which are then also used to GetDeviceQueue
    */


    while (!triangle->window->should_close()) {
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

