#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Meshoui
{
    struct SwapChainImageBufferVk final
    {
        ~SwapChainImageBufferVk();
        SwapChainImageBufferVk();

        VkImage       back;
        VkImageView   backView;
        VkFramebuffer front;
    };
    inline SwapChainImageBufferVk::~SwapChainImageBufferVk() {}
    inline SwapChainImageBufferVk::SwapChainImageBufferVk() : back(VK_NULL_HANDLE), backView(VK_NULL_HANDLE), front(VK_NULL_HANDLE) {}

    struct SwapChainCommandBufferVk final
    {
        ~SwapChainCommandBufferVk();
        SwapChainCommandBufferVk();

        VkCommandPool   commandPool;
        VkCommandBuffer commandBuffer;
        VkFence         fence;
        VkSemaphore     imageAcquiredSemaphore;
        VkSemaphore     renderCompleteSemaphore;
    };
    inline SwapChainCommandBufferVk::~SwapChainCommandBufferVk() {}
    inline SwapChainCommandBufferVk::SwapChainCommandBufferVk() : commandPool(VK_NULL_HANDLE), commandBuffer(VK_NULL_HANDLE), fence(VK_NULL_HANDLE), imageAcquiredSemaphore(VK_NULL_HANDLE), renderCompleteSemaphore(VK_NULL_HANDLE) {}

    struct SwapChainVk final
    {
        ~SwapChainVk();
        SwapChainVk();

        std::vector<SwapChainImageBufferVk> imageBuffers;
        std::vector<SwapChainCommandBufferVk> commandBuffers;
    };
    inline SwapChainVk::~SwapChainVk() {}
    inline SwapChainVk::SwapChainVk() : imageBuffers(), commandBuffers() {}
}
