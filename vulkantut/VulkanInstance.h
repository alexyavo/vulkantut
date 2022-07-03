#pragma once

#include "PhysDevice.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;


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
