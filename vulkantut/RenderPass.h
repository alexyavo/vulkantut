#pragma once

#include "LogicalDevice.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;

class RenderPass {
  VkRenderPass render_pass;
  ptr<LogicalDevice> device;

public:
  VkRenderPass get() { return render_pass; }

  RenderPass(ptr<LogicalDevice> device, VkFormat format) : device(device) {
    cout << "RenderPass() ctor\n";

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    create_info.attachmentCount = 1;
    create_info.pAttachments = &colorAttachment;
    create_info.subpassCount = 1;
    create_info.pSubpasses = &subpass;

    if (vkCreateRenderPass(device->get(), &create_info, nullptr, &render_pass) != VK_SUCCESS) {
      throw runtime_error("failed to create render pass");
    }
  }

  ~RenderPass() {
    cout << "~RenderPass()\n";
    vkDestroyRenderPass(device->get(), render_pass, nullptr);
  }
};
