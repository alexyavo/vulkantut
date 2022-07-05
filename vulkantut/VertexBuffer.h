#pragma once

#include "LogicalDevice.h"
#include "Vertex.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;


class VertexBuffer {
  ptr<LogicalDevice> device;
  VkBuffer buffer;
  VkDeviceMemory memory;
  vector<Vertex> verts;

public:
  VkBuffer get() { return buffer; }

  u32 size() { return verts.size(); }

  VertexBuffer(ptr<LogicalDevice> device, const vector<Vertex>& verts)
    : device(device)
    , verts(verts)
  {
    VkBufferCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = sizeof(verts[0]) * verts.size();

    // bitwise or for multiple usages
    info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    // buffers can be owned by a specific queue family or be shared between
    // multiple at the same time
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device->get(), &info, nullptr, &buffer) != VK_SUCCESS) {
      throw runtime_error("failed to create vertex buffer");
    }

    VkMemoryRequirements memreqs;
    vkGetBufferMemoryRequirements(device->get(), buffer, &memreqs);

    VkMemoryAllocateInfo meminfo{};
    meminfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    meminfo.allocationSize = memreqs.size;
    meminfo.memoryTypeIndex = device->find_mem_type(
      memreqs.memoryTypeBits, 
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |  // able to map and write to it from the CPU 
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT   // copy immediately (see below) 
    );

    /*
    *   Unfortunately the driver may not immediately copy the data into the
    *   buffer memory, for example because of caching. It is also possible that
    *   writes to the buffer are not visible in the mapped memory yet. There
    *   are two ways to deal with that problem:
    * 
    * 1. Use a memory heap that is host coherent, indicated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 
    * 2. Call vkFlushMappedMemoryRanges after writing to the mapped memory, and
    *    call vkInvalidateMappedMemoryRanges before reading from the mapped memory
    * 
    * Flushing memory ranges or using a coherent memory heap means that the
    * driver will be aware of our writes to the buffer, but it doesn't mean
    * that they are actually visible on the GPU yet. The transfer of data to
    * the GPU is an operation that happens in the background and the
    * specification simply tells us that it is guaranteed to be complete as of
    * the next call to vkQueueSubmit.
    */

    if (vkAllocateMemory(device->get(), &meminfo, nullptr, &memory) != VK_SUCCESS) {
      throw runtime_error("failed to allocate vertex buffer memory");
    }

    vkBindBufferMemory(device->get(), buffer, memory, 0);

    void* data;
    vkMapMemory(device->get(), memory, 0, memreqs.size, 0, &data);
    memcpy(data, verts.data(), memreqs.size);
    vkUnmapMemory(device->get(), memory);
  }

  ~VertexBuffer() {
    vkDestroyBuffer(device->get(), buffer, nullptr);
    vkFreeMemory(device->get(), memory, nullptr);
  }
};
