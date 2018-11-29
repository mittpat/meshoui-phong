#pragma once

#include <vulkan/vulkan.h>

namespace Meshoui
{
    struct DeviceBuffer
    {
        ~DeviceBuffer();
        DeviceBuffer();

        VkBuffer buffer;
        VkDeviceMemory bufferMemory;
        VkDeviceSize bufferSize;
    };
    inline DeviceBuffer::~DeviceBuffer() {}
    inline DeviceBuffer::DeviceBuffer() : buffer(VK_NULL_HANDLE), bufferMemory(VK_NULL_HANDLE), bufferSize(VK_NULL_HANDLE) {}

    struct RenderDevice
    {
        ~RenderDevice();
        RenderDevice();
        RenderDevice(VkPhysicalDevice p, VkDevice d, VkAllocationCallbacks* a);
        uint32_t memoryType(VkMemoryPropertyFlags properties, uint32_t type_bits);
        void createBuffer(DeviceBuffer &deviceBuffer, size_t size, VkBufferUsageFlags usage);
        void uploadBuffer(const DeviceBuffer &deviceBuffer, VkDeviceSize size, const void *data);
        void deleteBuffer(const DeviceBuffer &deviceBuffer);

        VkPhysicalDevice physicalDevice;
        VkDevice device;
        VkAllocationCallbacks* allocator;
        VkDeviceSize bufferMemoryAlignment;
    };
    inline RenderDevice::~RenderDevice() {}
    inline RenderDevice::RenderDevice() : physicalDevice(VK_NULL_HANDLE), device(VK_NULL_HANDLE), allocator(VK_NULL_HANDLE), bufferMemoryAlignment(256) {}
    inline RenderDevice::RenderDevice(VkPhysicalDevice p, VkDevice d, VkAllocationCallbacks *a = VK_NULL_HANDLE) : physicalDevice(p), device(d), allocator(a), bufferMemoryAlignment(256) {}
}
