#include "RenderDevice.h"

#include <cstdio>
#include <cstdlib>
#include <string.h>

namespace
{
    void check_vk_result(VkResult err)
    {
        if (err == 0) return;
        printf("VkResult %d\n", err);
        if (err < 0)
            abort();
    }

    uint32_t memoryType(VkPhysicalDevice physicalDevice, VkMemoryPropertyFlags properties, uint32_t type_bits)
    {
        VkPhysicalDeviceMemoryProperties prop;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &prop);
        for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
            if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1<<i))
                return i;
        return 0xFFFFFFFF;
    }
}

using namespace Meshoui;

void RenderDevice::createBuffer(DeviceBuffer &deviceBuffer, size_t size, VkBufferUsageFlags usage)
{
    VkResult err;
    if (deviceBuffer.buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, deviceBuffer.buffer, allocator);
    if (deviceBuffer.bufferMemory)
        vkFreeMemory(device, deviceBuffer.bufferMemory, allocator);

    VkDeviceSize vertex_buffer_size_aligned = ((size - 1) / bufferMemoryAlignment + 1) * bufferMemoryAlignment;
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = vertex_buffer_size_aligned;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    err = vkCreateBuffer(device, &buffer_info, allocator, &deviceBuffer.buffer);
    check_vk_result(err);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, deviceBuffer.buffer, &req);
    bufferMemoryAlignment = (bufferMemoryAlignment > req.alignment) ? bufferMemoryAlignment : req.alignment;
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = memoryType(physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
    err = vkAllocateMemory(device, &alloc_info, allocator, &deviceBuffer.bufferMemory);
    check_vk_result(err);

    err = vkBindBufferMemory(device, deviceBuffer.buffer, deviceBuffer.bufferMemory, 0);
    check_vk_result(err);
    deviceBuffer.bufferSize = size;
}

void RenderDevice::uploadBuffer(const DeviceBuffer &deviceBuffer, VkDeviceSize size, const void *data)
{
    VkResult err;
    {
        void* dest = nullptr;
        err = vkMapMemory(device, deviceBuffer.bufferMemory, 0, size, 0, &dest);
        check_vk_result(err);
        memcpy(dest, data, size);
    }
    VkMappedMemoryRange range = {};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = deviceBuffer.bufferMemory;
    range.size = VK_WHOLE_SIZE;
    err = vkFlushMappedMemoryRanges(device, 1, &range);
    check_vk_result(err);
    vkUnmapMemory(device, deviceBuffer.bufferMemory);
}
