#include "SwapChainVk.h"
#include "DeviceVk.h"

#include <cstdio>
#include <cstdlib>

namespace
{
    void check_vk_result(VkResult err)
    {
        if (err == 0) return;
        printf("VkResult %d\n", err);
        if (err < 0)
            abort();
    }
}

using namespace Meshoui;

void SwapChainVk::createCommandBuffers(DeviceVk &device, uint32_t frameCount)
{
    frames.resize(frameCount);

    VkResult err;
    for (auto & frame : frames)
    {
        {
            VkCommandPoolCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            info.queueFamilyIndex = device.queueFamily;
            err = vkCreateCommandPool(device.device, &info, device.allocator, &frame.pool);
            check_vk_result(err);
        }
        {
            VkCommandBufferAllocateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            info.commandPool = frame.pool;
            info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            info.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(device.device, &info, &frame.buffer);
            check_vk_result(err);
        }
        {
            VkFenceCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            err = vkCreateFence(device.device, &info, device.allocator, &frame.fence);
            check_vk_result(err);
        }
        {
            VkSemaphoreCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            err = vkCreateSemaphore(device.device, &info, device.allocator, &frame.acquired);
            check_vk_result(err);
            err = vkCreateSemaphore(device.device, &info, device.allocator, &frame.complete);
            check_vk_result(err);
        }
    }
}

void SwapChainVk::destroyCommandBuffers(DeviceVk &device)
{
    vkQueueWaitIdle(device.queue);
    for (auto & frame : frames)
    {
        vkDestroyFence(device.device, frame.fence, device.allocator);
        vkFreeCommandBuffers(device.device, frame.pool, 1, &frame.buffer);
        vkDestroyCommandPool(device.device, frame.pool, device.allocator);
        vkDestroySemaphore(device.device, frame.acquired, device.allocator);
        vkDestroySemaphore(device.device, frame.complete, device.allocator);
    }
}

void SwapChainVk::createImageBuffers()
{

}

void SwapChainVk::destroyImageBuffers()
{

}

VkSemaphore& SwapChainVk::beginRender(const DeviceVk &device, VkSwapchainKHR swapChainKHR, VkRenderPass renderPass, uint32_t &frameIndex, const VkExtent2D &extent)
{
    VkResult err;

    VkSemaphore& imageAcquiredSemaphore  = frames[frameIndex].acquired;
    {
        err = vkAcquireNextImageKHR(device.device, swapChainKHR, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, &frameIndex);
        check_vk_result(err);

        err = vkWaitForFences(device.device, 1, &frames[frameIndex].fence, VK_TRUE, UINT64_MAX);	// wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(device.device, 1, &frames[frameIndex].fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(device.device, frames[frameIndex].pool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(frames[frameIndex].buffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = renderPass;
        info.framebuffer = images[frameIndex].front;
        info.renderArea.extent = extent;
        VkClearValue clearValue[2] = {0};
        clearValue[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
        clearValue[1].depthStencil = {1.0f, 0};
        info.pClearValues = clearValue;
        info.clearValueCount = 2;
        vkCmdBeginRenderPass(frames[frameIndex].buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    return imageAcquiredSemaphore;
}

VkResult SwapChainVk::endRender(VkSemaphore &imageAcquiredSemaphore, VkSwapchainKHR swapChainKHR, VkQueue queue, uint32_t &frameIndex)
{
    vkCmdEndRenderPass(frames[frameIndex].buffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &imageAcquiredSemaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &frames[frameIndex].buffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &frames[frameIndex].complete;

        VkResult err = vkEndCommandBuffer(frames[frameIndex].buffer);
        check_vk_result(err);
        err = vkQueueSubmit(queue, 1, &info, frames[frameIndex].fence);
        check_vk_result(err);
    }

    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &frames[frameIndex].complete;
    info.swapchainCount = 1;
    info.pSwapchains = &swapChainKHR;
    info.pImageIndices = &frameIndex;
    return vkQueuePresentKHR(queue, &info);
}
