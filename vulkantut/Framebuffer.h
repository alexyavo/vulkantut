#pragma once

#include "LogicalDevice.h"
#include "RenderPass.h"
#include "ImageView.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;


class Framebuffer {
  ptr<LogicalDevice> device;
  ptr<RenderPass> renderpass;
  ptr<ImageView> view;

public:
  VkFramebuffer buffer;

  Framebuffer(
    ptr<LogicalDevice> device,
    ptr<RenderPass> renderpass, 
    ptr<ImageView> view, 
    VkExtent2D extent
  ) 
    : device(device)
    , renderpass(renderpass) 
    , view(view)
  {
    cout << "Framebuffer() ctor\n";
    
    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = renderpass->get();

    VkImageView attachments[] = { view->get() };
    create_info.attachmentCount = 1;
    create_info.pAttachments = attachments;

    create_info.width = extent.width;
    create_info.height = extent.height;
    create_info.layers = 1;

    if (vkCreateFramebuffer(device->get(), &create_info, nullptr, &buffer) != VK_SUCCESS) {
      throw runtime_error("failed to create framebuffer");
    }
  }

  ~Framebuffer() {
    cout << "~Framebuffer()\n";
    vkDestroyFramebuffer(device->get(), buffer, nullptr);
  }
};
