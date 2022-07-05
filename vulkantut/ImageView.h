#pragma once

#include "LogicalDevice.h"
#include "Swapchain.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;

class ImageView {
  VkImageView view;
  ptr<LogicalDevice> device;
  ptr<Swapchain> swapchain;

public:
  ImageView(
    ptr<LogicalDevice> device,
    ptr<Swapchain> swapchain,
    VkImage image,
    VkFormat format
  ) : device(device)
    , swapchain(swapchain)
  {
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;

    create_info.image = image;
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format = format;
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device->get(), &create_info, nullptr, &view) != VK_SUCCESS) {
      throw runtime_error("failed to create image view");
    }
  }

  ~ImageView() {
    vkDestroyImageView(device->get(), view, nullptr);
  }

  VkImageView get() { return view; }
};
