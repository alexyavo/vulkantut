#pragma once

#include "LogicalDevice.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;

class Fence {
  ptr<LogicalDevice> device;
  VkFence fence;

public:
  VkFence get() { return fence; }

  Fence(ptr<LogicalDevice> device): device(device) {
    VkFenceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(device->get(), &create_info, nullptr, &fence) != VK_SUCCESS) {
      throw runtime_error("failed to create fence");
    }
  }

  ~Fence() {
    vkDestroyFence(device->get(), fence, nullptr);
  }
};
