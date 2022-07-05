#pragma once

#include <glm/glm.hpp>
#include <array>

#include "vulkan_include.h"

using namespace std;

struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;

  static VkVertexInputBindingDescription binding_desc() {
    VkVertexInputBindingDescription desc{};
    desc.binding = 0; // index of binding in array of bindings
    desc.stride = sizeof(Vertex); // num byes from one entry to the next
    desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;  // move to the next data entry after each vertex (vs. "instanced rendering" (?))

    return desc;
  }

  static array<VkVertexInputAttributeDescription, 2> attr_desc() {
    array<VkVertexInputAttributeDescription, 2> desc{};
    /*
    float: VK_FORMAT_R32_SFLOAT
    vec2: VK_FORMAT_R32G32_SFLOAT
    vec3: VK_FORMAT_R32G32B32_SFLOAT
    vec4: VK_FORMAT_R32G32B32A32_SFLOAT

    ivec2: VK_FORMAT_R32G32_SINT, a 2-component vector of 32-bit signed integers
    uvec4: VK_FORMAT_R32G32B32A32_UINT, a 4-component vector of 32-bit unsigned integers
    double: VK_FORMAT_R64_SFLOAT, a double-precision (64-bit) float
    */

    desc[0].binding = 0;
    desc[0].location = 0;
    desc[0].format = VK_FORMAT_R32G32_SFLOAT;
    desc[0].offset = offsetof(Vertex, pos);

    desc[1].binding = 0;
    desc[1].location = 1;
    desc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    desc[1].offset = offsetof(Vertex, color);

    return desc;
  }
};
