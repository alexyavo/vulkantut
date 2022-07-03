#pragma once

#include <vector>
#include <string>

#include "QueueFamily.h"

#include "vulkan_include.h"
#include "utils.h"

using namespace std;
using namespace utils;


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
