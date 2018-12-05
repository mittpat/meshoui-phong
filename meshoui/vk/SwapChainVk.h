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
        VkImageView   view;
        VkFramebuffer front;
    };
    inline SwapChainImageBufferVk::~SwapChainImageBufferVk() {}
    inline SwapChainImageBufferVk::SwapChainImageBufferVk() : back(VK_NULL_HANDLE), view(VK_NULL_HANDLE), front(VK_NULL_HANDLE) {}

    struct SwapChainCommandBufferVk final
    {
        ~SwapChainCommandBufferVk();
        SwapChainCommandBufferVk();

        VkCommandPool   pool;
        VkCommandBuffer buffer;
        VkFence         fence;
        VkSemaphore     acquired;
        VkSemaphore     complete;
    };
    inline SwapChainCommandBufferVk::~SwapChainCommandBufferVk() {}
    inline SwapChainCommandBufferVk::SwapChainCommandBufferVk() : pool(VK_NULL_HANDLE), buffer(VK_NULL_HANDLE), fence(VK_NULL_HANDLE), acquired(VK_NULL_HANDLE), complete(VK_NULL_HANDLE) {}

    struct DeviceVk;
    struct SwapChainVk final
    {
        ~SwapChainVk();
        SwapChainVk();

        VkSemaphore &beginRender(const DeviceVk &device, VkSwapchainKHR swapChainKHR, VkRenderPass renderPass, uint32_t &frameIndex, const VkExtent2D &extent);
        VkResult endRender(VkSemaphore &imageAcquiredSemaphore, VkSwapchainKHR swapChainKHR, VkQueue queue, uint32_t &frameIndex);

        std::vector<SwapChainImageBufferVk> images;
        std::vector<SwapChainCommandBufferVk> frames;
    };
    inline SwapChainVk::~SwapChainVk() {}
    inline SwapChainVk::SwapChainVk() : images(), frames() {}
}
