#include "utils.h"

#include "Window.h"
#include "VulkanInstance.h"
#include "Surface.h"
#include "LogicalDevice.h"
#include "Swapchain.h"
#include "Framebuffer.h"
#include "GraphicsPipeline.h"
#include "Frame.h"
#include "Command.h"
#include "ImageView.h"

using namespace std;
using namespace utils;


vector<ptr<ImageView>> imageviews(ptr<LogicalDevice> device, ptr<Swapchain> swapchain) {
  vector<VkImage> images;

  u32 images_size;
  vkGetSwapchainImagesKHR(device->get(), swapchain->get(), &images_size, nullptr);
  images.resize(images_size);

  // TODO should this be freed in some way? 
  vkGetSwapchainImagesKHR(device->get(), swapchain->get(), &images_size, images.data());

  return map(
    images, 
    [=](const VkImage& image) {
      return mk_ptr<ImageView>(device, swapchain, image, swapchain->format);
    }
  );
}

vector<ptr<Framebuffer>> framebuffers(ptr<LogicalDevice> device, ptr<Swapchain> swapchain, ptr<RenderPass> renderpass) {
  return map(
    imageviews(device, swapchain), 
    [=](auto& view) {
      return mk_ptr<Framebuffer>(device, renderpass, view, swapchain->extent);
    }
  );
}


class BetterTriangle {
public:
  ptr<Window> window;
  ptr<VulkanInstance> instance;
  ptr<Surface> surface;
  ptr<PhysDevice> physical_device;
  ptr<LogicalDevice> device;
  ptr<Swapchain> swapchain;
  vector<ptr<Framebuffer>> framebuffers;
  ptr<RenderPass> renderpass;
  ptr<Command> command;
  ptr<GraphicsPipeline> pipeline;
  vector<ptr<Frame>> frames;
  vector<ptr<Frame>>::iterator curr_frame;

  const u32 max_frames_inflight = 2;

  static ptr<PhysDevice> find_physical_device(ptr<VulkanInstance> instance, ptr<Surface> surface) {
    auto suitable_physical_devices = instance->find_devices([surface](const PhysDevice& device) {
      return
        !device.surface_formats(surface->get()).empty() &&
        !device.surface_present_modes(surface->get()).empty() &&
        !device.graphics_queue_families().empty() &&
        !device.present_queue_families(surface->get()).empty() &&
        device.supports_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
      }
    );

    if (suitable_physical_devices.empty()) {
      throw runtime_error("failed to find suitable GPU");
    }

    cout << format(
      "suitable physical devices: {}\n",
      to_str(map(suitable_physical_devices, [](auto& d) { return d.name(); }), "\n\t", ",\n\t")
    );

    // should be ok? The vulkan handle is copied over...
    return mk_ptr<PhysDevice>(suitable_physical_devices.back().get());
  }

private:
  void init_swapchain() {
    cout << "... initializing swap chain\n";

    window->wait_minimized(); 
    device->wait_idle(); // this prevents destroying framebuffers that are being used...

    // not sure which of these fuckers causes it, but without fully destroying these before
    // recreating them, I get heap corruption (specifically happened when destroying swapchain,
    // so maybe we can't have multiple swapchains at the same time or something?)
    swapchain = nullptr;
    renderpass = nullptr;
    framebuffers.clear();
    pipeline = nullptr;

    swapchain = mk_ptr<Swapchain>(device, surface);
    renderpass = mk_ptr<RenderPass>(device, swapchain->format);
    framebuffers = ::framebuffers(device, swapchain, renderpass);
    pipeline = mk_ptr<GraphicsPipeline>(device, swapchain, renderpass);
  }

public:
  BetterTriangle(uint32_t height, uint32_t width) {
    window = mk_ptr<Window>(height, width);
    instance = mk_ptr<VulkanInstance>(true);
    surface = mk_ptr<Surface>(instance, window);

    physical_device = find_physical_device(instance, surface);

    QueueFamily graphics_fam = physical_device->graphics_queue_families().back();
    QueueFamily present_fam = physical_device->present_queue_families(surface->get()).back();
    device = mk_ptr<LogicalDevice>(
      physical_device,
      graphics_fam,
      present_fam
    );

    command = mk_ptr<Command>(device, graphics_fam.index, max_frames_inflight);
    init_swapchain();

    for (u32 i = 0; i < max_frames_inflight; ++i) {
      frames.push_back(mk_ptr<Frame>(device));
    }

    curr_frame = frames.begin();
  }

  void run() {
    while (!window->should_close()) {
      glfwPollEvents();

      VkResult draw_result = (*curr_frame)->draw(
        renderpass,
        swapchain,
        pipeline,
        framebuffers,
        command->get_buffer(curr_frame - frames.begin())
      );

      if (draw_result == VK_ERROR_OUT_OF_DATE_KHR ||
          draw_result == VK_SUBOPTIMAL_KHR ||
          window->check_resize()
      ) {
        init_swapchain();
      } else if (draw_result != VK_SUCCESS) {
        throw runtime_error("failed to present swap chain image");
      }

      if (++curr_frame == frames.end()) {
        curr_frame = frames.begin();
      }
    }

    device->wait_idle();
  }
};


int main() {
  try {
    auto triangle = mk_ptr<BetterTriangle>(800, 600);
    triangle->run();
  } catch (const exception& ex) {
    cerr << ex.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

