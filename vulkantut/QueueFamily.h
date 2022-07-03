#pragma once

#include "vulkan_include.h"
#include "utils.h"

using namespace utils;

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
  const u32 index;

  QueueFamily(VkQueueFamilyProperties properties, u32 index) : properties(properties), index(index) {}

  bool supports_graphics() const {
    return properties.queueFlags & VK_QUEUE_GRAPHICS_BIT;
  }
};

