#pragma once

#include "LogicalDevice.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;

class Sema {
  ptr<LogicalDevice> device;
  VkSemaphore sema;

public:
  VkSemaphore get() { return sema; }

  Sema(ptr<LogicalDevice> device): device(device) {
    VkSemaphoreCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(device->get(), &create_info, nullptr, &sema) != VK_SUCCESS) {
      throw runtime_error("failed to create semaphore");
    }
  }

  ~Sema() {
    vkDestroySemaphore(device->get(), sema, nullptr);
  }
};
