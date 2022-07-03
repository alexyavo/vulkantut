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

using namespace std;
using namespace utils;


class BetterTriangle {
public:
  ptr<Window> window;
  ptr<VulkanInstance> instance;
  ptr<Surface> surface; 

  // this devision of pointers and non-pointers... ugh
  // depends on your understanding of whether the object has RAII associated with it
  // or if, like PhysDevice, it's just a wrapper around a Vulkan handle (ptr) that 
  // wraps it in a nicer interface
  ptr<PhysDevice> physical_device; 

  ptr<LogicalDevice> device;
  ptr<Swapchain> swapchain;
  vector<ptr<Framebuffer>> framebuffers;

  ptr<RenderPass> renderpass;
  ptr<Command> command;
  ptr<GraphicsPipeline> pipeline;

  vector<ptr<Frame>> frames;
  vector<ptr<Frame>>::iterator curr_frame;

  const u32 max_frames_inflight = 1;

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
    device->wait_idle(); // resize issue...
    cout << "... initializing swap chain\n";

    swapchain = mk_ptr<Swapchain>(device, surface->get());
    renderpass = mk_ptr<RenderPass>(device, swapchain->format);
    framebuffers = swapchain->framebuffers(renderpass);
    pipeline = mk_ptr<GraphicsPipeline>(swapchain, renderpass);
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

      VkResult draw_result = (*curr_frame)->draw_frame(
        renderpass,
        swapchain,
        pipeline,
        framebuffers,
        command->get_buffer(curr_frame - frames.begin())
      );
      
      switch (draw_result) {
      case VK_SUCCESS:
        break;

      case VK_ERROR_OUT_OF_DATE_KHR:
      case VK_SUBOPTIMAL_KHR:
        cout << "VK_SUBOPTIMAL_KHR || VK_ERROR_OUT_OF_DATE_KHR\n";
        init_swapchain();
        continue;

      default:
        throw runtime_error("failed to present swap chain image");
      }

      if (window->check_resize()) {
        init_swapchain();
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

