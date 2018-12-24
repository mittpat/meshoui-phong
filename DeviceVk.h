#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Meshoui
{
    struct DeviceBufferVk final
    {
        ~DeviceBufferVk();
        DeviceBufferVk();

        VkBuffer buffer;
        VkDeviceMemory memory;
        VkDeviceSize size;
    };
    inline DeviceBufferVk::~DeviceBufferVk() {}
    inline DeviceBufferVk::DeviceBufferVk() : buffer(VK_NULL_HANDLE), memory(VK_NULL_HANDLE), size(VK_NULL_HANDLE) {}

    struct ImageBufferVk final
    {
        ~ImageBufferVk();
        ImageBufferVk();

        VkImage image;
        VkDeviceMemory memory;
        VkImageView view;
    };
    inline ImageBufferVk::~ImageBufferVk() {}
    inline ImageBufferVk::ImageBufferVk() : image(VK_NULL_HANDLE), memory(VK_NULL_HANDLE), view(VK_NULL_HANDLE) {}

    struct InstanceVk;
    struct DeviceVk final
    {
        ~DeviceVk();
        DeviceVk();

        void create(InstanceVk &instance);
        void destroy();
        void selectSurfaceFormat(VkSurfaceKHR &surface, VkSurfaceFormatKHR &surfaceFormat, const std::vector<VkFormat> &request_formats, VkColorSpaceKHR request_color_space);
        void createBuffer(DeviceBufferVk &deviceBuffer, VkDeviceSize size, VkBufferUsageFlags usage);
        void uploadBuffer(const DeviceBufferVk &deviceBuffer, VkDeviceSize size, const void *data);
        void deleteBuffer(const DeviceBufferVk &deviceBuffer);

        void createBuffer(ImageBufferVk &deviceBuffer, const VkExtent3D &extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask);
        void transferBuffer(const DeviceBufferVk &fromBuffer, ImageBufferVk &toBuffer, const VkExtent3D & extent, VkCommandBuffer commandBuffer);
        void deleteBuffer(const ImageBufferVk &deviceBuffer);

        VkPhysicalDevice             physicalDevice;
        VkDevice                     device;
        uint32_t                     queueFamily;
        VkQueue                      queue;
        VkDescriptorPool             descriptorPool;
        const VkAllocationCallbacks* allocator;
        VkDeviceSize                 memoryAlignment;

    private:
        uint32_t memoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
    };
    inline DeviceVk::~DeviceVk() {}
    inline DeviceVk::DeviceVk() : physicalDevice(VK_NULL_HANDLE), device(VK_NULL_HANDLE), queueFamily(-1), queue(VK_NULL_HANDLE), descriptorPool(VK_NULL_HANDLE), allocator(VK_NULL_HANDLE), memoryAlignment(256) {}
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