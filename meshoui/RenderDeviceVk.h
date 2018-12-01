#pragma once

#include <vulkan/vulkan.h>

namespace Meshoui
{
    struct DeviceBufferVk final
    {
        ~DeviceBufferVk();
        DeviceBufferVk();

        VkBuffer buffer;
        VkDeviceMemory bufferMemory;
        VkDeviceSize bufferSize;
    };
    inline DeviceBufferVk::~DeviceBufferVk() {}
    inline DeviceBufferVk::DeviceBufferVk() : buffer(VK_NULL_HANDLE), bufferMemory(VK_NULL_HANDLE), bufferSize(VK_NULL_HANDLE) {}

    struct ImageBufferVk final
    {
        ~ImageBufferVk();
        ImageBufferVk();

        VkImage image;
        VkDeviceMemory imageMemory;
        VkImageView imageView;
    };
    inline ImageBufferVk::~ImageBufferVk() {}
    inline ImageBufferVk::ImageBufferVk() : image(VK_NULL_HANDLE), imageMemory(VK_NULL_HANDLE), imageView(VK_NULL_HANDLE) {}

    struct RenderDeviceVk final
    {
        ~RenderDeviceVk();
        RenderDeviceVk();
        RenderDeviceVk(VkPhysicalDevice p, VkDevice d, VkAllocationCallbacks* a);
        uint32_t memoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
        void createBuffer(DeviceBufferVk &deviceBuffer, size_t size, VkBufferUsageFlags usage);
        void uploadBuffer(const DeviceBufferVk &deviceBuffer, VkDeviceSize size, const void *data);
        void deleteBuffer(const DeviceBufferVk &deviceBuffer);

        void createBuffer(ImageBufferVk &deviceBuffer, const VkExtent3D &extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask);
        void deleteBuffer(const ImageBufferVk &deviceBuffer);

        VkPhysicalDevice physicalDevice;
        VkDevice device;
        VkAllocationCallbacks* allocator;
        VkDeviceSize bufferMemoryAlignment;
    };
    inline RenderDeviceVk::~RenderDeviceVk() {}
    inline RenderDeviceVk::RenderDeviceVk() : physicalDevice(VK_NULL_HANDLE), device(VK_NULL_HANDLE), allocator(VK_NULL_HANDLE), bufferMemoryAlignment(256) {}
    inline RenderDeviceVk::RenderDeviceVk(VkPhysicalDevice p, VkDevice d, VkAllocationCallbacks *a = VK_NULL_HANDLE) : physicalDevice(p), device(d), allocator(a), bufferMemoryAlignment(256) {}
}
