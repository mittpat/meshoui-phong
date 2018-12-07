#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#define FrameCount 2

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
    struct ImageBufferVk;
    struct SwapChainVk final
    {
        ~SwapChainVk();
        SwapChainVk();

        void createCommandBuffers(DeviceVk &device);
        void destroyCommandBuffers(DeviceVk &device);
        void createImageBuffers(DeviceVk &device, ImageBufferVk &depthBuffer, VkSurfaceKHR surface, VkSurfaceFormatKHR surfaceFormat, int w, int h, bool vsync);
        void destroyImageBuffers(DeviceVk &device, ImageBufferVk &depthBuffer);

        VkSemaphore &beginRender(const DeviceVk &device, uint32_t &frameIndex);
        VkResult endRender(VkSemaphore &imageAcquiredSemaphore, VkQueue queue, uint32_t &frameIndex);

        std::vector<SwapChainImageBufferVk> images;
        std::vector<SwapChainCommandBufferVk> frames;

        VkSwapchainKHR swapChainKHR;
        VkRenderPass   renderPass;
        VkExtent2D     extent;
    };
    inline SwapChainVk::~SwapChainVk() {}
    inline SwapChainVk::SwapChainVk() : images(), frames(), swapChainKHR(VK_NULL_HANDLE), renderPass(VK_NULL_HANDLE), extent({0, 0}) {}
}
