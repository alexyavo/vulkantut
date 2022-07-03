#pragma once

#include "LogicalDevice.h"
#include "ImageView.h"
#include "RenderPass.h"
#include "Framebuffer.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;


class Swapchain {
public:
  ptr<LogicalDevice> device;

  VkSwapchainKHR swapchain;
  vector<VkImage> images;
  VkFormat format;
  VkExtent2D extent;

  VkSwapchainKHR get() { return swapchain; }

  ~Swapchain() {
    cout << "~SwapChain()\n";
    vkDestroySwapchainKHR(device->get(), swapchain, nullptr);
  }

  // convention:
  // pass what you mean to store a ref to internally by ptr
  // pass vk handles you don't intend to use / store outside of ctor by direct type
  // pass const ref to objects you need the wrapper functions for
  // this is fucked tho, better to not pass vk handles at all..
  Swapchain(ptr<LogicalDevice> device, VkSurfaceKHR surface): device(device) {
    cout << "SwapChain() ctor\n";

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface;

    auto surface_formats = device->physical_device->surface_formats(surface);
    auto surface_format= find(
      surface_formats,
      [](const VkSurfaceFormatKHR& fmt) {
        return
          fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
          fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      }
    ).value_or(surface_formats[0]);

    format = surface_format.format;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;

    auto surface_capabilities = device->physical_device->surface_capabilities(surface); 

    // TODO (?)
    extent = surface_capabilities.currentExtent;
    create_info.imageExtent = surface_capabilities.currentExtent;

    create_info.minImageCount = surface_capabilities.minImageCount + 1; // TODO capabilities.maxImageCount

    //We can specify that a certain transform should be applied to images in the
    //swap chain if it is supported (supportedTransforms in capabilities), like
    //a 90 degree clockwise rotation or horizontal flip. To specify that you do
    //not want any transformation, simply specify the current transformation.
    create_info.preTransform = surface_capabilities.currentTransform;

    // This is always 1 unless you are developing a stereoscopic 3D application.
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // TODO assumes graphics & present family are the same
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    // The compositeAlpha field specifies if the alpha channel should be used
    // for blending with other windows in the window system. You'll almost
    // always want to simply ignore the alpha channel, hence
    // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // The presentMode member speaks for itself. If the clipped member is set to
    // VK_TRUE then that means that we don't care about the color of pixels that are
    // obscured, for example because another window is in front of them. Unless you
    // really need to be able to read these pixels back and get predictable results,
    // you'll get the best performance by enabling clipping.
    create_info.presentMode = find(
      device->physical_device->surface_present_modes(surface),
      [](const VkPresentModeKHR& mode) { return mode == VK_PRESENT_MODE_MAILBOX_KHR; }
    ).value_or(VK_PRESENT_MODE_FIFO_KHR);

    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device->get(), &create_info, nullptr, &swapchain) != VK_SUCCESS) {
      throw runtime_error("failed to create swap chain");
    }

    u32 images_size;
    vkGetSwapchainImagesKHR(device->get(), swapchain, &images_size, nullptr);
    images.resize(images_size);

    vkGetSwapchainImagesKHR(device->get(), swapchain, &images_size, images.data());
  }

  vector<ptr<ImageView>> imageviews() {
    return map(images, [this](const VkImage& image) {
      return mk_ptr<ImageView>(device, image, format);
    });
  }

  vector<ptr<Framebuffer>> framebuffers(ptr<RenderPass> renderpass) {
    return map(imageviews(), [this, renderpass](auto& view) { return mk_ptr<Framebuffer>(device, renderpass, view, extent); });
  }
};
