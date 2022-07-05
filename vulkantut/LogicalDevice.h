#pragma once

#include "PhysDevice.h"
#include "QueueFamily.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;


class LogicalDevice {
  VkDevice device;

public:
  ptr<PhysDevice> physical_device;
  
  VkQueue graphics_q;
  VkQueue present_q;

  VkDevice get() { return device; }

  ~LogicalDevice() {
    vkDestroyDevice(device, nullptr);
  }

  LogicalDevice(
    ptr<PhysDevice> physical_device, 
    const QueueFamily& graphics_queue_family,
    const QueueFamily& present_queue_family
  ) 
    : physical_device(physical_device) 
  {
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    float default_q_priority = 1.0f;
    // the use of to_vector here is because we need a vector result, and result type is determined based on
    // the type of input collection. 
    // but... internally map() also uses push_back which isn't available for set()... 
    vector<VkDeviceQueueCreateInfo> queue_create_infos = map(
      to_vector(set<u32> { graphics_queue_family.index, present_queue_family.index }),
      // note that the priority is passed as pointer, so we have to get the reference to the local variable
      // otherwise we'll be passing a pointer to a lambda-local var 
      [&default_q_priority](u32 qfam_index) { return vk_queue_create_info(qfam_index, &default_q_priority); }
    );

    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.queueCreateInfoCount = static_cast<u32>(queue_create_infos.size());

    VkPhysicalDeviceFeatures features{};
    create_info.pEnabledFeatures = &features;

    // TODO hardcoded 
    vector<const char*> device_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    create_info.enabledExtensionCount = static_cast<u32>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();

    // TODO hardcoded
    vector<const char*> validation_layers = {
      "VK_LAYER_KHRONOS_validation"
    };
    create_info.enabledLayerCount = static_cast<u32>(validation_layers.size());
    create_info.ppEnabledLayerNames = validation_layers.data();

    if (vkCreateDevice(physical_device->get(), &create_info, nullptr, &device) != VK_SUCCESS) {
      throw runtime_error("failed to create logical device");
    }

    vkGetDeviceQueue(device, graphics_queue_family.index, 0, &graphics_q);
    vkGetDeviceQueue(device, present_queue_family.index, 0, &present_q);
  }

  void wait_fences(vector<VkFence>& fences) {
    vkWaitForFences(device, fences.size(), fences.data(), VK_TRUE, UINT64_MAX);
  }

  void reset_fences(vector<VkFence>& fences) {
    vkResetFences(device, fences.size(), fences.data());
  }

  void wait_idle() {
    vkDeviceWaitIdle(device);
  }

  u32 find_mem_type(u32 type_filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memprops;
    vkGetPhysicalDeviceMemoryProperties(physical_device->get(), &memprops);

    for (u32 i = 0; i < memprops.memoryTypeCount; ++i) {
      if ((type_filter & (1 << i)) && (memprops.memoryTypes[i].propertyFlags & props) == props) {
        return i;
      }
    }

    throw runtime_error("failed to find suitable memory type");
  }
};
