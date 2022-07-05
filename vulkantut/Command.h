#pragma once

#include "LogicalDevice.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;

class Command {
  ptr<LogicalDevice> device;
  VkCommandPool pool;

  vector<VkCommandBuffer> buffers;

public:
  Command(ptr<LogicalDevice> device, u32 qfam_index, u32 num_buffers) : device(device) {
    VkCommandPoolCreateInfo pool_create_info{};
    pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_create_info.queueFamilyIndex = qfam_index;

    if (vkCreateCommandPool(device->get(), &pool_create_info, nullptr, &pool) != VK_SUCCESS) {
      throw std::runtime_error("failed to create command pool");
    }

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = num_buffers;

    buffers.resize(num_buffers);
    if (vkAllocateCommandBuffers(device->get(), &alloc_info, buffers.data()) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate command buffers");
    }
  }

  VkCommandBuffer get_buffer(u32 i) { return buffers[i]; }

  ~Command() {
    //command buffers are freed for us when we free the command pool (?)
    //vkFreeCommandBuffers(device->get(), pool, buffers.size(), buffers.data());

    vkDestroyCommandPool(device->get(), pool, nullptr);
  }
};
