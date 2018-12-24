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