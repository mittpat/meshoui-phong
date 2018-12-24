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

void SwapChainVk::createCommandBuffers(DeviceVk &device)
{
    frames.resize(FrameCount);

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

void SwapChainVk::createImageBuffers(DeviceVk &device, ImageBufferVk &depthBuffer, VkSurfaceKHR surface, VkSurfaceFormatKHR surfaceFormat, int w, int h, bool vsync)
{
    VkResult err;
    VkSwapchainKHR old_swapchain = swapChainKHR;
    err = vkDeviceWaitIdle(device.device);
    check_vk_result(err);

    for (auto & image : images)
    {
        vkDestroyImageView(device.device, image.view, device.allocator);
        vkDestroyFramebuffer(device.device, image.front, device.allocator);
    }
    images.resize(0);
    if (renderPass)
    {
        vkDestroyRenderPass(device.device, renderPass, device.allocator);
    }

    {
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = surface;
        info.minImageCount = FrameCount;
        info.imageFormat = surfaceFormat.format;
        info.imageColorSpace = surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
        info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = old_swapchain;
        VkSurfaceCapabilitiesKHR cap;
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, surface, &cap);
        check_vk_result(err);
        if (info.minImageCount < cap.minImageCount)
            info.minImageCount = cap.minImageCount;
        else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
            info.minImageCount = cap.maxImageCount;

        if (cap.currentExtent.width == 0xffffffff)
        {
            info.imageExtent.width = extent.width = w;
            info.imageExtent.height = extent.height = h;
        }
        else
        {
            info.imageExtent.width = extent.width = cap.currentExtent.width;
            info.imageExtent.height = extent.height = cap.currentExtent.height;
        }
        err = vkCreateSwapchainKHR(device.device, &info, device.allocator, &swapChainKHR);
        check_vk_result(err);
        uint32_t backBufferCount = 0;
        err = vkGetSwapchainImagesKHR(device.device, swapChainKHR, &backBufferCount, NULL);
        check_vk_result(err);
        std::vector<VkImage> backBuffer(backBufferCount);
        err = vkGetSwapchainImagesKHR(device.device, swapChainKHR, &backBufferCount, backBuffer.data());
        check_vk_result(err);

        images.resize(backBufferCount);
        for (size_t i = 0; i < images.size(); ++i)
        {
            images[i].back = backBuffer[i];
        }
    }
    if (old_swapchain)
    {
        vkDestroySwapchainKHR(device.device, old_swapchain, device.allocator);
    }

    {
        VkAttachmentDescription attachment[2] = {};
        attachment[0].format = surfaceFormat.format;
        attachment[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment[1].format = VK_FORMAT_D16_UNORM;
        attachment[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depth_attachment = {};
        depth_attachment.attachment = 1;
        depth_attachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        subpass.pDepthStencilAttachment = &depth_attachment;

        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        err = vkCreateRenderPass(device.device, &info, device.allocator, &renderPass);
        check_vk_result(err);
    }
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = surfaceFormat.format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        info.subresourceRange = image_range;
        for (size_t i = 0; i < images.size(); ++i)
        {
            info.image = images[i].back;
            err = vkCreateImageView(device.device, &info, device.allocator, &images[i].view);
            check_vk_result(err);
        }
    }

    // depth buffer
    device.createBuffer(depthBuffer, {extent.width, extent.height, 1}, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    {
        VkImageView attachment[2] = {0, depthBuffer.view};
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderPass;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.width = extent.width;
        info.height = extent.height;
        info.layers = 1;
        for (size_t i = 0; i < images.size(); ++i)
        {
            attachment[0] = images[i].view;
            err = vkCreateFramebuffer(device.device, &info, device.allocator, &images[i].front);
            check_vk_result(err);
        }
    }
}

void SwapChainVk::destroyImageBuffers(DeviceVk &device, ImageBufferVk &depthBuffer)
{
    vkQueueWaitIdle(device.queue);
    device.deleteBuffer(depthBuffer);
    for (auto & image : images)
    {
        vkDestroyImageView(device.device, image.view, device.allocator);
        vkDestroyFramebuffer(device.device, image.front, device.allocator);
    }
    vkDestroyRenderPass(device.device, renderPass, device.allocator);
    vkDestroySwapchainKHR(device.device, swapChainKHR, device.allocator);
}

VkSemaphore& SwapChainVk::beginRender(const DeviceVk &device, uint32_t &frameIndex)
{
    VkResult err;

    VkSemaphore& imageAcquiredSemaphore  = frames[frameIndex].acquired;
    {
        err = vkAcquireNextImageKHR(device.device, swapChainKHR, UINT64_MAX, imageAcquiredSemaphore, VK_NULL_HANDLE, &frameIndex);
        check_vk_result(err);

        err = vkWaitForFences(device.device, 1, &frames[frameIndex].fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
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
        VkClearValue clearValue[2] = {};
        clearValue[0].color = {{0.1f, 0.1f, 0.2f, 1.0f}};
        clearValue[1].depthStencil = {1.0f, 0};
        info.pClearValues = clearValue;
        info.clearValueCount = 2;
        vkCmdBeginRenderPass(frames[frameIndex].buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    return imageAcquiredSemaphore;
}

VkResult SwapChainVk::endRender(VkSemaphore &imageAcquiredSemaphore, VkQueue queue, uint32_t &frameIndex)
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

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2018 Patrick Pelletier
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
