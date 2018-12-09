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

        VkPhysicalDevice       physicalDevice;
        VkDevice               device;
        uint32_t               queueFamily;
        VkQueue                queue;
        VkDescriptorPool       descriptorPool;
        VkAllocationCallbacks* allocator;
        VkDeviceSize           memoryAlignment;

    private:
        uint32_t memoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
    };
    inline DeviceVk::~DeviceVk() {}
    inline DeviceVk::DeviceVk() : physicalDevice(VK_NULL_HANDLE), device(VK_NULL_HANDLE), queueFamily(-1), queue(VK_NULL_HANDLE), descriptorPool(VK_NULL_HANDLE), allocator(VK_NULL_HANDLE), memoryAlignment(256) {}
}
