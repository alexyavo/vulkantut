#pragma once

#include "LogicalDevice.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;

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
