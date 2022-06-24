#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <cstdint> // Necessary for uint32_t
#include <limits> // Necessary for std::numeric_limits
#include <algorithm> // Necessary for std::clamp


#include <stdint.h> // numeric_limits<uint32_t>::max() doesn't work, use UINT_MAX instead

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <optional>
#include <set>
#include <fstream>
#include <format>
#include <functional>
#include <algorithm>
#include <sstream>

#include "utils.h"

using namespace std;
using namespace utils;

vector<VkLayerProperties> vk_get_layers() {
    uint32_t size = 0;
    vkEnumerateInstanceLayerProperties(&size, nullptr);

    vector<VkLayerProperties> layers(size);
    vkEnumerateInstanceLayerProperties(&size, layers.data());

    return layers;
}

vector<VkExtensionProperties> vk_get_extensions() {
  uint32_t size = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &size, nullptr);

  vector<VkExtensionProperties> extensions(size);
  vkEnumerateInstanceExtensionProperties(nullptr, &size, extensions.data());

  return extensions;
}

vector<const char*> glfw_required_extensions() {
  // Vulkan is a platform agnostic API, which means that you need an extension to interface with 
  // the window system. GLFW has a handy built-in function that returns the extension(s) it needs 
  // to do that which we can pass to the struct:

  /*
   *  @pointer_lifetime The returned array is allocated and freed by GLFW.  You
   *  should not free it yourself.  It is guaranteed to be valid only until the
   *  library is terminated.
   */
  uint32_t size = 0;
  const char** extensions = glfwGetRequiredInstanceExtensions(&size);
  return vector<const char*>(extensions, extensions + size);
}


static VKAPI_ATTR VkBool32 VKAPI_CALL
dbg_cb(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData
) {
  if (
    //severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
    ) {
    cout << format("[vkdbgcb] {}\n", pCallbackData->pMessage);
  }

  return VK_FALSE;
}


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

class PhysDevice {
  VkPhysicalDevice device;

public:
  PhysDevice(VkPhysicalDevice device) : device(device) {}

  VkPhysicalDevice get() { return device; }

  vector<VkExtensionProperties> get_extensions() const {
    uint32_t size = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &size, nullptr);

    vector<VkExtensionProperties> extensions(size);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &size, extensions.data());

    return extensions;
  }

  bool supports_extension(const char* extension) const {
    for (auto& ext : get_extensions()) {
      if (strcmp(ext.extensionName, extension) == 0) {
        return true;
      }
    }

    return false;
  }

  VkPhysicalDeviceProperties properties() const {
    VkPhysicalDeviceProperties p;
    vkGetPhysicalDeviceProperties(device, &p);
    return p;
  }

  string name() const {
    return string(properties().deviceName);
  }

  vector<QueueFamily> queue_families() const {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    vector<VkQueueFamilyProperties> properties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, properties.data());

    return map_enumerated(
      properties, 
      [](u32 index, VkQueueFamilyProperties props) { return QueueFamily(props, index); }
    );
  }

  vector<QueueFamily> graphics_queue_families() const {
    return filter(
      queue_families(),
      [](const QueueFamily& qfam) { return qfam.supports_graphics(); }
    );
  }

  vector<QueueFamily> present_queue_families(VkSurfaceKHR surface) const {
    return filter(
      queue_families(),
      [this, surface](const QueueFamily& qfam) { return present_support(surface, qfam.index); }
    );
  }

  bool present_support(VkSurfaceKHR surface, uint32_t queue_family_idx) const {
    VkBool32 support = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, queue_family_idx, surface, &support);
    return support;
  }

  VkSurfaceCapabilitiesKHR surface_capabilities(VkSurfaceKHR surface) const {
    VkSurfaceCapabilitiesKHR result;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &result);
    return result;
  }

  vector<VkSurfaceFormatKHR> surface_formats(VkSurfaceKHR surface) const {
    uint32_t size;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &size, nullptr);

    vector<VkSurfaceFormatKHR> formats(size);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &size, formats.data());

    return formats;
  }

  vector<VkPresentModeKHR> surface_present_modes(VkSurfaceKHR surface) const {
    uint32_t size;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &size, nullptr);

    vector<VkPresentModeKHR> modes(size);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &size, modes.data());
      
    return modes;
  }
};

class Window {
  GLFWwindow* glfw_window;

public:
  const int w, h;

  GLFWwindow* get() { return glfw_window; }

  bool should_close() {
    return glfwWindowShouldClose(glfw_window);
  }

  Window(int w, int h): w(w), h(h) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); 
    glfw_window = glfwCreateWindow(w, h, "Vulkan", nullptr, nullptr);
  }

  ~Window() {
    glfwDestroyWindow(glfw_window);
    glfwTerminate();
  }
};



class VulkanInstance {
private:
  const vector<const char *> validation_layers = {
    "VK_LAYER_KHRONOS_validation"
  };

  static VkApplicationInfo mk_app_info() {
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;
    return app_info;
  }

  static uptr<VkDebugUtilsMessengerCreateInfoEXT> mk_dbg_info(PFN_vkDebugUtilsMessengerCallbackEXT dbg_cb) {
    auto res = mk_uptr<VkDebugUtilsMessengerCreateInfoEXT>();
    *res = {};

    res->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    res->messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    res->messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    res->pfnUserCallback = dbg_cb;
    res->pUserData = nullptr; // Optional

    return res;
  }

private:
  uptr<VkDebugUtilsMessengerCreateInfoEXT> dbg_info;
  VkInstance instance;

public:
  VkInstance get() {
    return instance;
  }

public:
  VulkanInstance(bool debug = false) {
    VkInstanceCreateInfo instance_info{};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    auto app_info = mk_app_info();
    instance_info.pApplicationInfo = &app_info;

    vector<const char*> extensions = glfw_required_extensions();

    if (debug) {
      // validation layers
      {
        auto vk_layers_names = to_set(map(vk_get_layers(), [](auto layer) { return string(layer.layerName); }));
        auto validation_layers_names = to_set(map(validation_layers, [](auto vl) { return string(vl); }));
        if (!is_subset_of(validation_layers_names, vk_layers_names)) {
          throw runtime_error(format(
            "Required validation layers {} could not be found among available ones: {}",
            to_str(validation_layers_names),
            to_str(vk_layers_names)
          ));
        }

        cout << format("validation layers: \n{}\n", to_str(validation_layers_names, "\t", ",\n\t", ""));

        instance_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        instance_info.ppEnabledLayerNames = validation_layers.data();

        dbg_info = mk_dbg_info(dbg_cb);
        instance_info.pNext = dbg_info.get();
      }

      // extensions
      {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        auto extensions_names = to_set(map(extensions, [](auto e) { return string(e); }));
        auto vk_extensions_names = to_set(map(vk_get_extensions(), [](auto ext) { return string(ext.extensionName); }));
        if (!is_subset_of(extensions_names, vk_extensions_names)) {
          throw runtime_error(format("Required extensions {} could be found among available ones: {}",
            to_str(extensions_names),
            to_str(vk_extensions_names)
          ));
        }

        cout << format("extensions: \n{}\n", to_str(extensions_names, "\t", ",\n\t", ""));
      }
    }

    instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instance_info.ppEnabledExtensionNames = extensions.data();
   
    if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
      throw runtime_error("Failed to create instance");
    }
  }

  ~VulkanInstance() {
    vkDestroyInstance(instance, nullptr);
  }

public:
  // should this return pointers? PhysDevice is a wrapper around a vulkan handle (VkPhysicalDevice) 
  // so it works this way but...  
  vector<PhysDevice> find_devices(function<bool(const PhysDevice&)> pred) {
    uint32_t size = 0;
    vkEnumeratePhysicalDevices(instance, &size, nullptr);

    vector<VkPhysicalDevice> devices(size);
    vkEnumeratePhysicalDevices(instance, &size, devices.data());

    return filter(map(devices, [](VkPhysicalDevice d) { return PhysDevice(d); }), pred);
  }
};

class Surface {
private:
  VkSurfaceKHR surface;
  ptr<VulkanInstance> instance;
  ptr<Window> window;

public:
  Surface(ptr<VulkanInstance> instance, ptr<Window> window): instance(instance), window(window) {
    if (glfwCreateWindowSurface(instance->get(), window->get(), nullptr, &surface) != VK_SUCCESS) {
      throw runtime_error("failed to create window surface");
    }
  }

  ~Surface() {
    vkDestroySurfaceKHR(instance->get(), surface, nullptr);
  }

public:
  VkSurfaceKHR get() { return surface; }
};

VkDeviceQueueCreateInfo vk_queue_create_info(u32 family_index, const float* priority) {
  VkDeviceQueueCreateInfo res{};
  res.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  res.queueFamilyIndex = family_index;
  res.queueCount = 1;
  res.pQueuePriorities = priority;
  return res;
}

class LogicalDevice {
  VkDevice device;

public:
  ptr<PhysDevice> physical_device;
  
  VkQueue graphics_q;
  VkQueue present_q;

  VkDevice get() { return device; }

  ~LogicalDevice() {
    vkDestroyDevice(device, nullptr);
  }

  LogicalDevice(
    ptr<PhysDevice> physical_device, 
    const QueueFamily& graphics_queue_family,
    const QueueFamily& present_queue_family
  ) 
    : physical_device(physical_device) 
  {
    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    float default_q_priority = 1.0f;
    // the use of to_vector here is because we need a vector result, and result type is determined based on
    // the type of input collection. 
    // but... internally map() also uses push_back which isn't available for set()... 
    vector<VkDeviceQueueCreateInfo> queue_create_infos = map(
      to_vector(set<u32> { graphics_queue_family.index, present_queue_family.index }),
      // note that the priority is passed as pointer, so we have to get the reference to the local variable
      // otherwise we'll be passing a pointer to a lambda-local var 
      [&default_q_priority](u32 qfam_index) { return vk_queue_create_info(qfam_index, &default_q_priority); }
    );

    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.queueCreateInfoCount = static_cast<u32>(queue_create_infos.size());

    VkPhysicalDeviceFeatures features{};
    create_info.pEnabledFeatures = &features;

    // TODO hardcoded 
    vector<const char*> device_extensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    create_info.enabledExtensionCount = static_cast<u32>(device_extensions.size());
    create_info.ppEnabledExtensionNames = device_extensions.data();

    // TODO hardcoded
    vector<const char*> validation_layers = {
      "VK_LAYER_KHRONOS_validation"
    };
    create_info.enabledLayerCount = static_cast<u32>(validation_layers.size());
    create_info.ppEnabledLayerNames = validation_layers.data();

    if (vkCreateDevice(physical_device->get(), &create_info, nullptr, &device) != VK_SUCCESS) {
      throw runtime_error("failed to create logical device");
    }

    vkGetDeviceQueue(device, graphics_queue_family.index, 0, &graphics_q);
    vkGetDeviceQueue(device, present_queue_family.index, 0, &present_q);
  }

  void wait_fences(vector<VkFence>& fences) {
    vkWaitForFences(device, fences.size(), fences.data(), VK_TRUE, UINT64_MAX);
  }

  void reset_fences(vector<VkFence>& fences) {
    vkResetFences(device, fences.size(), fences.data());
  }
};

class ImageView {
  VkImageView view;
  ptr<LogicalDevice> device;

public:
  ImageView(ptr<LogicalDevice> device, VkImage image, VkFormat format) : device(device) {
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

class RenderPass {
  VkRenderPass render_pass;
  ptr<LogicalDevice> device;

public:
  VkRenderPass get() { return render_pass; }

  RenderPass(ptr<LogicalDevice> device, VkFormat format) : device(device) {
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
    vkDestroyRenderPass(device->get(), render_pass, nullptr);
  }
};

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
    vkDestroyFramebuffer(device->get(), buffer, nullptr);
  }
};

class SwapChain {
public:
  ptr<LogicalDevice> device;

  VkSwapchainKHR swapchain;
  vector<VkImage> images;
  VkFormat format;
  VkExtent2D extent;

  VkSwapchainKHR get() { return swapchain; }

  ~SwapChain() {
    vkDestroySwapchainKHR(device->get(), swapchain, nullptr);
  }

  // convention:
  // pass what you mean to store a ref to internally by ptr
  // pass vk handles you don't intend to use / store outside of ctor by direct type
  // pass const ref to objects you need the wrapper functions for
  // this is fucked tho, better to not pass vk handles at all..
  SwapChain(ptr<LogicalDevice> device, VkSurfaceKHR surface): device(device) {
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

class Shader {
  ptr<LogicalDevice> device;
  vector<char> code; 

  VkShaderModule shader;

public:
  VkShaderModule get() { return shader; }

  Shader(ptr<LogicalDevice> device, const char* fname) : device(device) {
    // Unlike earlier APIs, shader code in Vulkan has to be specified in a
    // bytecode format as opposed to human-readable syntax like GLSL and HLSL.
    // This bytecode format is called SPIR-V and is designed to be used with both
    // Vulkan and OpenCL (both Khronos APIs)
    code = read_file(fname);
      
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const u32*>(code.data());

    if (vkCreateShaderModule(device->get(), &create_info, nullptr, &shader) != VK_SUCCESS) {
      throw runtime_error("failed to create shader module");
    }
  }

  ~Shader() {
    vkDestroyShaderModule(device->get(), shader, nullptr);
  }

  // There is one more (optional) member, pSpecializationInfo, which we won't
  // be using here, but is worth discussing. It allows you to specify values
  // for shader constants. You can use a single shader module where its
  // behavior can be configured at pipeline creation by specifying different
  // values for the constants used in it. This is more efficient than
  // configuring the shader using variables at render time, because the
  // compiler can do optimizations like eliminating if statements that depend
  // on these values. If you don't have any constants like that, then you can
  // set the member to nullptr, which our struct initialization does
  // automatically.
  VkPipelineShaderStageCreateInfo pipeline_stage(VkShaderStageFlagBits stage) {
    VkPipelineShaderStageCreateInfo res{};
    res.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    res.stage = stage;
    res.module = shader; // specify the code to run
    res.pName = "main"; // entrypoint of in the code
    return res;
  }
};

class Command : public enable_shared_from_this<Command> {
  ptr<LogicalDevice> device;
  VkCommandPool pool;

public:
  vector<VkCommandBuffer> buffers;

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

//class CommandBuffer {
//  VkCommandBuffer buffer;
//public:
//  VkCommandBuffer get() { return buffer; }
//
//  CommandBuffer(VkCommandBuffer buffer) : buffer(buffer) {}
//};

class GraphicsPipeline {
  ptr<SwapChain> swapchain;
  ptr<RenderPass> renderpass;

  VkPipelineLayout layout;
  VkPipeline pipeline;
public:
  VkPipeline get() { return pipeline; }

  GraphicsPipeline(ptr<SwapChain> swapchain, ptr<RenderPass> renderpass) 
    : swapchain(swapchain)
    , renderpass(renderpass)
  {
    // describes the format of the vertex data that will be passed to the vertex shader.
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

    // describes two things: what kind of geometry will be drawn from the
    // vertices and if primitive restart should be enabled
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // A viewport basically describes the region of the framebuffer that the
    // output will be rendered to. This will almost always be (0, 0) to (width,
    // height)
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchain->extent.width);
    viewport.height = static_cast<float>(swapchain->extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // While viewports define the transformation from the image to the
    // framebuffer, scissor rectangles define in which regions pixels will
    // actually be stored. Any pixels outside the scissor rectangles will be
    // discarded by the rasterizer. 
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchain->extent;

    // ewport and scissor rectangle need to be combined into a viewport state 
    // It is possible to use multiple viewports and scissor rectangles on some
    // graphics cards, so its members reference an array of them.
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // rasterizer takes the geometry that is shaped by the vertices from the vertex
    // shader and turns it into fragments to be colored by the fragment shader. It
    // also performs depth testing, face culling and the scissor test, and it can
    // be configured to output fragments that fill entire polygons or just the
    // edges (wireframe rendering)
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;

    // If rasterizerDiscardEnable is set to VK_TRUE, then geometry never passes
    // through the rasterizer stage. This basically disables any output to the
    // framebuffer.
    rasterizer.rasterizerDiscardEnable = VK_FALSE;

    rasterizer.polygonMode = VK_POLYGON_MODE_FILL; // how fragments are generated for geometry
    rasterizer.lineWidth = 1.0f; // thickness of lines in terms of number of fragments

    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f; // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    // multisampling == one of the way the ways to perform antialiasing
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = 
      VK_COLOR_COMPONENT_R_BIT |
      VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT |
      VK_COLOR_COMPONENT_A_BIT;

    colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    //colorBlendAttachment.blendEnable = VK_TRUE;
    //colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    //colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    //colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    //colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    //colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    //colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    vector<VkDynamicState> dynamicStates = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_LINE_WIDTH
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0; // Optional
    pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
    pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
    pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(swapchain->device->get(), &pipelineLayoutInfo, nullptr, &layout) != VK_SUCCESS) {
      throw runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;

    Shader vert(swapchain->device, "shaders/vert.spv");
    Shader frag(swapchain->device, "shaders/frag.spv");
    VkPipelineShaderStageCreateInfo stages[] = {
      vert.pipeline_stage(VK_SHADER_STAGE_VERTEX_BIT), 
      frag.pipeline_stage(VK_SHADER_STAGE_FRAGMENT_BIT) 
    };
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;

    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = nullptr; // Optional
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = nullptr; // Optional
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = renderpass->get();
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    if (vkCreateGraphicsPipelines(swapchain->device->get(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS) {
      throw runtime_error("failed to create graphics pipeline!");
    }
  }

  ~GraphicsPipeline() {
    vkDestroyPipelineLayout(swapchain->device->get(), layout, nullptr);
    vkDestroyPipeline(swapchain->device->get(), pipeline, nullptr);
  }
};

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

class Frame {
  ptr<LogicalDevice> device;

  ptr<Sema> image_available_sema;
  ptr<Sema> render_finished_sema;
  ptr<Fence> inflight_fence;

public:
  Frame(
    ptr<LogicalDevice> device
  ) : device(device)
  { 
    image_available_sema = mk_ptr<Sema>(device);
    render_finished_sema = mk_ptr<Sema>(device);
    inflight_fence = mk_ptr<Fence>(device);
  }

  ~Frame() {

  }

  void draw_frame(
    ptr<RenderPass> renderpass,
    ptr<SwapChain> swapchain,
    ptr<GraphicsPipeline> pipeline,
    vector<ptr<Framebuffer>> framebuffers,
    VkCommandBuffer buffer
  ) {
    // At a high level, rendering a frame in Vulkan consists of a common set of steps:
    // - Wait for the previous frame to finish
    // - Acquire an image from the swap chain
    // - Record a command buffer which draws the scene onto that image
    // - Submit the recorded command buffer
    // - Present the swap chain image

    vector<VkFence> fences { inflight_fence->get() };
    device->wait_fences(fences);
    device->reset_fences(fences);

    uint32_t image_index;
    vkAcquireNextImageKHR(
      device->get(),
      swapchain->get(),
      UINT64_MAX,
      image_available_sema->get(),
      VK_NULL_HANDLE,
      &image_index
    );

    vkResetCommandBuffer(buffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(buffer, &beginInfo) != VK_SUCCESS) {
      throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderpass->get();
    renderPassInfo.framebuffer = framebuffers[image_index]->buffer;

    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapchain->extent;

    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->get());
    vkCmdDraw(buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(buffer);

    if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
      throw std::runtime_error("failed to record command buffer!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { image_available_sema->get() };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &buffer;

    VkSemaphore signalSemaphores[] = { render_finished_sema->get() };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(device->graphics_q, 1, &submitInfo, inflight_fence->get()) != VK_SUCCESS) {
      throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapchain->get() };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &image_index;

    vkQueuePresentKHR(device->present_q, &presentInfo);
  }
};

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
  ptr<SwapChain> swapchain;
  vector<ptr<Framebuffer>> framebuffers;

  ptr<RenderPass> renderpass;
  ptr<Command> command;
  ptr<GraphicsPipeline> pipeline;

  vector<ptr<Frame>> frames;
  vector<ptr<Frame>>::iterator curr_frame;

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

    swapchain = mk_ptr<SwapChain>(device, surface->get());

    // SwapChain has: 
    // --------------
    // VkSwapchainKHR swapchain;
    // vector<VkImage> images;
    // VkFormat format;
    // VkExtent2D extent;

    //mk_ptr<RenderPass>(); // RenderPass(ptr<LogicalDevice> device, VkFormat format) 
    //mk_ptr<GraphicsPipeline>(swapchain,); // GraphicsPipeline(ptr<SwapChain> swapchain, ptr<RenderPass> render_pass) 
    //mk_ptr<Command>(); // Command(ptr<LogicalDevice> device, u32 qfam_index) : device(device) {

    renderpass = mk_ptr<RenderPass>(device, swapchain->format);
    framebuffers = swapchain->framebuffers(renderpass);
    
    pipeline = mk_ptr<GraphicsPipeline>(swapchain, renderpass);
    command = mk_ptr<Command>(device, graphics_fam.index, 2);
    
    //ptr<LogicalDevice> device,
    //ptr<SwapChain> swapchain,
    //ptr<GraphicsPipeline> pipeline,
    //vector<ptr<Framebuffer>> framebuffers,
    //VkCommandBuffer buffer
    frames.push_back(mk_ptr<Frame>(device));
    frames.push_back(mk_ptr<Frame>(device));

    curr_frame = frames.begin();
  }

  void run() {
    while (!window->should_close()) {
      glfwPollEvents();

      (*curr_frame)->draw_frame(
        renderpass,
        swapchain,
        pipeline,
        framebuffers,
        command->get_buffer(curr_frame - frames.begin())
      );

      if (++curr_frame == frames.end()) {
        curr_frame = frames.begin();
      }
    }

    vkDeviceWaitIdle(device->get());
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

