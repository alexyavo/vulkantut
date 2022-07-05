#pragma once

#include "LogicalDevice.h"
#include "Sema.h"
#include "Fence.h"
#include "RenderPass.h"
#include "Swapchain.h"
#include "GraphicsPipeline.h"
#include "VertexBuffer.h"

#include "vulkan_include.h"
#include "utils.h"
#include "vk_utils.h"

using namespace std;
using namespace utils;

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
    cout << "~Frame()\n";
  }

  VkResult draw(
    ptr<RenderPass> renderpass,
    ptr<Swapchain> swapchain,
    ptr<GraphicsPipeline> pipeline,
    vector<ptr<Framebuffer>> framebuffers,
    VkCommandBuffer buffer,
    ptr<VertexBuffer> vertices
  ) {
    // At a high level, rendering a frame in Vulkan consists of a common set of steps:
    // - Wait for the previous frame to finish
    // - Acquire an image from the swap chain
    // - Record a command buffer which draws the scene onto that image
    // - Submit the recorded command buffer
    // - Present the swap chain image

    vector<VkFence> fences { inflight_fence->get() };
    device->wait_fences(fences);

    uint32_t image_index;
    VkResult res = vkAcquireNextImageKHR(
      device->get(),
      swapchain->get(),
      UINT64_MAX,
      image_available_sema->get(),
      VK_NULL_HANDLE,
      &image_index
    );

    if (res != VK_SUCCESS) {
      cout << "vkAcquireNextImageKHR failed\n";
      return res;
    }

    // only reset if we're submitting work 
    // (see "Fixing a Deadlock" @ https://vulkan-tutorial.com/Drawing_a_triangle/Swap_chain_recreation)
    device->reset_fences(fences);  

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
    
    VkBuffer verticess[] = { vertices->get() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(buffer, 0, 1, verticess, offsets);

    vkCmdDraw(buffer, vertices->size(), 1, 0, 0);
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

    return vkQueuePresentKHR(device->present_q, &presentInfo);
  }
};
