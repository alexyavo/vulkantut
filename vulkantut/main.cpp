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

using namespace std;

/* The input assembler collects the raw vertex data from the buffers you specify
and may also use an index buffer to repeat certain elements without having to
duplicate the vertex data itself.

The vertex shader is run for every vertex and generally applies transformations
to turn vertex positions from model space to screen space. It also passes
per-vertex data down the pipeline.

The tessellation shaders allow you to subdivide geometry based on certain rules
to increase the mesh quality. This is often used to make surfaces like brick
walls and staircases look less flat when they are nearby.

The geometry shader is run on every primitive (triangle, line, point) and can
discard it or output more primitives than came in. This is similar to the
tessellation shader, but much more flexible. However, it is not used much in
today's applications because the performance is not that good on most graphics
cards except for Intel's integrated GPUs.

The rasterization stage discretizes the primitives into fragments. These are the
pixel elements that they fill on the framebuffer. Any fragments that fall
outside the screen are discarded and the attributes outputted by the vertex
shader are interpolated across the fragments, as shown in the figure. Usually
the fragments that are behind other primitive fragments are also discarded here
because of depth testing.

The fragment shader is invoked for every fragment that survives and determines
which framebuffer(s) the fragments are written to and with which color and depth
values. It can do this using the interpolated data from the vertex shader, which
can include things like texture coordinates and normals for lighting.

The color blending stage applies operations to mix different fragments that map
to the same pixel in the framebuffer. Fragments can simply overwrite each other,
add up or be mixed based upon transparency. */

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

const vector<const char*> validationLayers = {
  "VK_LAYER_KHRONOS_validation"
};

const vector<const char*> deviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};


#ifdef NDEBUG
const bool enableValidationLayers = false;
#else 
const bool enableValidationLayers = true;
#endif


vector<char> read_file(const string& fname) {
  ifstream file(fname, ios::ate | ios::binary);

  if (!file.is_open()) {
    throw runtime_error(format("failed to open {}", fname));
  }

  size_t fileSize = file.tellg();
  vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();

  return buffer;
}

set<string> to_set(const vector<const char*>& strs) {
  set<string> res{};
  for (auto s : strs) {
    res.insert(string(s));
  }
  return res;
}

// returns true if device supports all required extensions
bool deviceSupportsExtensions(VkPhysicalDevice device, const set<string>& required) {
  uint32_t extCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, nullptr);

  vector<VkExtensionProperties> availableExtensions(extCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extCount, availableExtensions.data());

  size_t found = 0; // TODO is size_t the correct type for counting? 
  for (const auto& e : availableExtensions) {
    if (required.contains(e.extensionName)) {
      found++;
    }
  }

  return found == required.size();
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const vector<VkSurfaceFormatKHR>& formats) {
  for (const auto& fmt : formats) {
    if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB && fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return fmt;
    }
  }

  return formats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const vector<VkPresentModeKHR>& availablePresentModes) {
  for (const auto& availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  VkDebugUtilsMessageTypeFlagsEXT messageType,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData
) {
  if (
    //    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT ||
    severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
    ) {
    cout << format("[dbgcb] {}\n", pCallbackData->pMessage);
  }

  return VK_FALSE;
}


/*
 * This struct (VkDebugUtilsMessengerCreateInfoEXT) should be passed to the
 * vkCreateDebugUtilsMessengerEXT function to create
 * the VkDebugUtilsMessengerEXT object. Unfortunately, because this function is an extension
 * function, it is not automatically loaded. We have to look up its address ourselves using
 * vkGetInstanceProcAddr. We're going to create our own proxy function that handles this in
 * the background.
 */
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

VkApplicationInfo mkAppInfo() {
  VkApplicationInfo res{};
  res.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  res.pApplicationName = "Hello Triangle";
  res.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  res.pEngineName = "No Engine";
  res.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  res.apiVersion = VK_API_VERSION_1_0;
  return res;
}

VkDebugUtilsMessengerCreateInfoEXT mkDbgInfo() {
  VkDebugUtilsMessengerCreateInfoEXT res = {};
  res.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  res.messageSeverity =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

  res.messageType =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  res.pfnUserCallback = debugCallback;
  res.pUserData = nullptr; // Optional
  return res;
}

static VkDeviceQueueCreateInfo mkQInfo(uint32_t familyIndex, const float* priority) {
  VkDeviceQueueCreateInfo res{};
  res.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  res.queueFamilyIndex = familyIndex;
  res.queueCount = 1;
  res.pQueuePriorities = priority;
  return res;
}

struct QueueFamilyIndices {
  optional<uint32_t> graphicsFamily;
  optional<uint32_t> presentFamily;

  bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities{};
  vector<VkSurfaceFormatKHR> formats;
  vector<VkPresentModeKHR> presentModes;
};

class HelloTriangleApplication {
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
  }

private: // main methods
  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create OpenGL context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // disbale window resizing
    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }

  void initVulkan() {
    createInstance();
    setupDbgMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffer();
    createSyncObjects();
  }

  void cleanup() {
    vkDestroySemaphore(ldevice, imageAvailableSemaphore, nullptr);
    vkDestroySemaphore(ldevice, renderFinishedSemaphore, nullptr);
    vkDestroyFence(ldevice, inFlightFence, nullptr);

    vkDestroyCommandPool(ldevice, commandPool, nullptr);

    for (auto fb : swapChainFramebuffers) {
      vkDestroyFramebuffer(ldevice, fb, nullptr);
    }

    vkDestroyPipeline(ldevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(ldevice, pipelineLayout, nullptr);
    vkDestroyRenderPass(ldevice, renderPass, nullptr);

    for (auto view : swapChainImageViews) {
      vkDestroyImageView(ldevice, view, nullptr);
    }

    vkDestroySwapchainKHR(ldevice, swapChain, nullptr);

    vkDestroyDevice(ldevice, nullptr);

    if (enableValidationLayers) {
      DestroyDebugUtilsMessengerEXT(instance, dbgMessenger, nullptr);
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();
  }

  void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      drawFrame();
    }

    vkDeviceWaitIdle(ldevice);
  }

private: // memebrs
  GLFWwindow* window;
  VkInstance instance;
  VkDebugUtilsMessengerEXT dbgMessenger;

  VkPhysicalDevice pdevice = VK_NULL_HANDLE;
  VkDevice ldevice;

  // Device queues are implicitly cleaned up when the device is destroyed, so we don't need to do anything in cleanup.
  VkQueue graphicsQueue;

  VkSurfaceKHR surface;
  VkQueue presentQueue;

  VkSwapchainKHR swapChain;
  vector<VkImage> swapChainImages;
  VkFormat swapChainFormat;
  VkExtent2D swapChainExtent;

  vector<VkImageView> swapChainImageViews;

  VkRenderPass renderPass;
  VkPipelineLayout pipelineLayout;
  VkPipeline graphicsPipeline;

  vector<VkFramebuffer> swapChainFramebuffers;
  
  VkCommandPool commandPool;
  VkCommandBuffer commandBuffer;

  VkSemaphore imageAvailableSemaphore;
  VkSemaphore renderFinishedSemaphore;
  VkFence inFlightFence;

private: // methods
  void drawFrame() {
    // At a high level, rendering a frame in Vulkan consists of a common set of steps:
    // - Wait for the previous frame to finish
    // - Acquire an image from the swap chain
    // - Record a command buffer which draws the scene onto that image
    // - Submit the recorded command buffer
    // - Present the swap chain image

    vkWaitForFences(ldevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(ldevice, 1, &inFlightFence);
    uint32_t imageIndex;
    vkAcquireNextImageKHR(ldevice, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    vkResetCommandBuffer(commandBuffer, 0);
    recordCommandBuffer(commandBuffer, imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS) {
      throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);
  }

  void createInstance() {
    if (enableValidationLayers && !checkValidationlayerSupport()) {
      throw runtime_error("validation layers requested, but not available!");
    }

    // A lot of information in Vulkan is passed through structs instead of function parameters and 
    // we'll have to fill in one more struct to provide sufficient information for creating an 
    // instance. This next struct is not optional and tells the Vulkan driver which global extensions 
    // and validation layers we want to use. Global here means that they apply to the entire program 
    // and not a specific device, which will become clear in the next few chapters.
    auto appInfo = mkAppInfo();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = extensions.size();
    createInfo.ppEnabledExtensionNames = extensions.data();

    // needs to be outside the if, because might be used in vkCreateInstance
    VkDebugUtilsMessengerCreateInfoEXT dbgCreateInfo = mkDbgInfo();
    if (enableValidationLayers) {
      createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
      createInfo.ppEnabledLayerNames = validationLayers.data();
      createInfo.pNext = &dbgCreateInfo;
    } else {
      createInfo.enabledLayerCount = 0;
      createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
      throw runtime_error("Failed to create instance");
    }
  }

  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
      return capabilities.currentExtent;
    } else {
      int width, height;
      glfwGetFramebufferSize(window, &width, &height);

      VkExtent2D actualExtent = {
          static_cast<uint32_t>(width),
          static_cast<uint32_t>(height)
      };

      actualExtent.width = clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
      actualExtent.height = clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

      return actualExtent;
    }
  }

  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);


    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) {
      details.formats.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
      details.presentModes.resize(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
  }

  bool checkValidationlayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    vector<VkLayerProperties> availLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availLayers.data());

    for (const char* layerName : validationLayers) {
      bool layerFound = false;

      for (const auto& layerProps : availLayers) {
        if (strcmp(layerName, layerProps.layerName) == 0) {
          layerFound = true;
          break;
        }
      }

      if (!layerFound) {
        return false;
      }
    }

    return true;
  }

  vector<const char*> getRequiredExtensions() {
    // Vulkan is a platform agnostic API, which means that you need an extension to interface with 
    // the window system. GLFW has a handy built-in function that returns the extension(s) it needs 
    // to do that which we can pass to the struct:
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    /*
     *  @pointer_lifetime The returned array is allocated and freed by GLFW.  You
     *  should not free it yourself.  It is guaranteed to be valid only until the
     *  library is terminated.
     */
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (enableValidationLayers) {
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    cout << "required extensions:\n";
    for (int i = 0; i < glfwExtensionCount; ++i) {
      cout << '\t' << glfwExtensions[i] << '\n';
    }

    {
      uint32_t extensionCount = 0;
      vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

      vector<VkExtensionProperties> extensions(extensionCount);
      vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

      cout << "available extensions:\n";
      for (const auto& ext : extensions) {
        cout << '\t' << ext.extensionName << '\n';
      }
    }

    return extensions;
  }

  void setupDbgMessenger() {
    if (enableValidationLayers) {
      return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo = mkDbgInfo();
    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &dbgMessenger) != VK_SUCCESS) {
      throw runtime_error("failed to set up debug messenger!");
    }
  }

  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    QueueFamilyIndices indices;
    for (uint32_t i = 0; i < queueFamilyCount && !indices.isComplete(); ++i) {
      if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        indices.graphicsFamily = i;
      }

      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
      if (presentSupport) {
        indices.presentFamily = i;
      }
    }

    return indices;
  }


  void createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(pdevice);

    vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    float queuePriority = 1.0f;
    for (uint32_t qFamily : uniqueQueueFamilies) {
      queueCreateInfos.push_back(mkQInfo(qFamily, &queuePriority));
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    VkDeviceCreateInfo cinfo{};
    cinfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    cinfo.pQueueCreateInfos = queueCreateInfos.data();
    cinfo.queueCreateInfoCount = queueCreateInfos.size();

    cinfo.pEnabledFeatures = &deviceFeatures;

    cinfo.enabledExtensionCount = deviceExtensions.size();
    cinfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (enableValidationLayers) {
      cinfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
      cinfo.ppEnabledLayerNames = validationLayers.data();
    } else {
      cinfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(pdevice, &cinfo, nullptr, &ldevice) != VK_SUCCESS) {
      throw runtime_error("failed to create logical device!");
    }

    vkGetDeviceQueue(ldevice, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(ldevice, indices.presentFamily.value(), 0, &presentQueue);
  }

  void createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
      throw runtime_error("failed to create window surface");
    }
  }

  void createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(pdevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(ldevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
      throw std::runtime_error("failed to create command pool!");
    }
  }

  void createCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(ldevice, &allocInfo, &commandBuffer) != VK_SUCCESS) {
      throw std::runtime_error("failed to allocate command buffers!");
    }
  }

  void recordCommandBuffer(VkCommandBuffer buffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
      throw std::runtime_error("failed to begin recording command buffer!");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];

    renderPassInfo.renderArea.offset = { 0, 0 };
    renderPassInfo.renderArea.extent = swapChainExtent;

    VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    vkCmdDraw(buffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(buffer);

    if (vkEndCommandBuffer(buffer) != VK_SUCCESS) {
      throw std::runtime_error("failed to record command buffer!");
    }
  }

  void createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(ldevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
      vkCreateSemaphore(ldevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
      vkCreateFence(ldevice, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
      throw std::runtime_error("failed to create semaphores!");
    }
  }

  void createFramebuffers() {
    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
      VkImageView attachments[] = {
          swapChainImageViews[i]
      };

      VkFramebufferCreateInfo framebufferInfo{};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = renderPass;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments = attachments;
      framebufferInfo.width = swapChainExtent.width;
      framebufferInfo.height = swapChainExtent.height;
      framebufferInfo.layers = 1;

      if (vkCreateFramebuffer(ldevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
        throw runtime_error("failed to create framebuffer!");
      }
    }
  }

  void createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapChainFormat;
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

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(ldevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
      throw runtime_error("failed to create render pass!");
    }
  }

  void createGraphicsPipeline() {
    // Unlike earlier APIs, shader code in Vulkan has to be specified in a
    // bytecode format as opposed to human-readable syntax like GLSL and HLSL.
    // This bytecode format is called SPIR-V and is designed to be used with both
    // Vulkan and OpenCL (both Khronos APIs)

    auto vertCode = read_file("shaders/vert.spv");
    auto fragCode = read_file("shaders/frag.spv");

    VkShaderModule vertShader = createShaderModule(vertCode);
    VkShaderModule fragShader = createShaderModule(fragCode);

    VkPipelineShaderStageCreateInfo vertStageInfo{};
    vertStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStageInfo.module = vertShader; // specify the code to run
    vertStageInfo.pName = "main"; // entrypoint of in the code

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

    VkPipelineShaderStageCreateInfo fragStageInfo{};
    fragStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStageInfo.module = fragShader; // specify the code to run
    fragStageInfo.pName = "main"; // entrypoint of in the code

    VkPipelineShaderStageCreateInfo stages[] = { vertStageInfo, fragStageInfo };

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
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    // While viewports define the transformation from the image to the
    // framebuffer, scissor rectangles define in which regions pixels will
    // actually be stored. Any pixels outside the scissor rectangles will be
    // discarded by the rasterizer. 
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;

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
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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

    if (vkCreatePipelineLayout(ldevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
      throw runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
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
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex = -1; // Optional

    if (vkCreateGraphicsPipelines(ldevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
      throw runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(ldevice, fragShader, nullptr);
    vkDestroyShaderModule(ldevice, vertShader, nullptr);
  }

  VkShaderModule createShaderModule(const vector<char>& code) {
    VkShaderModuleCreateInfo cinfo{};
    cinfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    cinfo.codeSize = code.size();
    cinfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shader;
    if (vkCreateShaderModule(ldevice, &cinfo, nullptr, &shader) != VK_SUCCESS) {
      throw runtime_error("failed to create shader module");
    }

    return shader;
  }

  void createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());
    for (size_t i = 0; i < swapChainImages.size(); ++i) {
      VkImageViewCreateInfo createInfo{};
      createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      createInfo.image = swapChainImages[i];
      createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
      createInfo.format = swapChainFormat;
      createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      createInfo.subresourceRange.baseMipLevel = 0;
      createInfo.subresourceRange.levelCount = 1;
      createInfo.subresourceRange.baseArrayLayer = 0;
      createInfo.subresourceRange.layerCount = 1;

      if (vkCreateImageView(ldevice, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
        throw runtime_error("failed to create image views!");
      }
    }
  }

  void createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pdevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    // However, simply sticking to this minimum means that we may sometimes have
    // to wait on the driver to complete internal operations before we can acquire
    // another image to render to. Therefore it is recommended to request at least
    // one more image than the minimum:
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {
      imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;

    // The imageArrayLayers specifies the amount of layers each image consists of.
    // This is always 1 unless you are developing a stereoscopic 3D application.
    createInfo.imageArrayLayers = 1;

    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = findQueueFamilies(pdevice);
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily) {
      createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      createInfo.queueFamilyIndexCount = 2;
      createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
      createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      createInfo.queueFamilyIndexCount = 0; // Optional
      createInfo.pQueueFamilyIndices = nullptr; // Optional
    }

    //We can specify that a certain transform should be applied to images in the
    //swap chain if it is supported (supportedTransforms in capabilities), like
    //a 90 degree clockwise rotation or horizontal flip. To specify that you do
    //not want any transformation, simply specify the current transformation.
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

    // The compositeAlpha field specifies if the alpha channel should be used
    // for blending with other windows in the window system. You'll almost
    // always want to simply ignore the alpha channel, hence
    // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // The presentMode member speaks for itself. If the clipped member is set to
    // VK_TRUE then that means that we don't care about the color of pixels that are
    // obscured, for example because another window is in front of them. Unless you
    // really need to be able to read these pixels back and get predictable results,
    // you'll get the best performance by enabling clipping.
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(ldevice, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
      throw runtime_error("failed to create swap chain");
    }

    vkGetSwapchainImagesKHR(ldevice, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(ldevice, swapChain, &imageCount, swapChainImages.data());
    swapChainFormat = surfaceFormat.format;
    swapChainExtent = extent;
  }

  void pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      throw runtime_error("failed to find GPU");
    }

    vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    for (const auto& d : devices) {
      if (isDeviceSuitable(d)) {
        pdevice = d;
        break;
      }
    }

    if (pdevice == VK_NULL_HANDLE) {
      throw runtime_error("failed to find suitable GPU");
    }
  }

  //	Not all graphics cards are capable of presenting images directly to a
  //	screen for various reasons, for example because they are designed for
  //	servers and don't have any display outputs. Secondly, since image
  //	presentation is heavily tied into the window system and the surfaces
  //	associated with windows, it is not actually part of the Vulkan core. You
  //	have to enable the VK_KHR_swapchain device extension after querying for
  //	its support.


  bool isDeviceSuitable(VkPhysicalDevice device) {
    //		VkPhysicalDeviceProperties props;
    //		VkPhysicalDeviceFeatures features;
    //		vkGetPhysicalDeviceProperties(device, &props);
    //		vkGetPhysicalDeviceFeatures(device, &features);
    //
    //		return props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
    //			features.geometryShader;

    if (!deviceSupportsExtensions(device, set(to_set(deviceExtensions)))) {
      return false;
    }

    QueueFamilyIndices indices = findQueueFamilies(device);
    SwapChainSupportDetails swap = querySwapChainSupport(device);

    return indices.isComplete() && !swap.formats.empty() && !swap.presentModes.empty();
  }
};

int main() {
  HelloTriangleApplication app;

  try {
    app.run();
  } catch (const exception& e) {
    cerr << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
