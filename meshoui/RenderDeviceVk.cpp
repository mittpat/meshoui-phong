#include "RenderDeviceVk.h"

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
}

using namespace Meshoui;

uint32_t RenderDeviceVk::memoryType(VkMemoryPropertyFlags properties, uint32_t type_bits)
{
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1<<i))
            return i;
    return 0xFFFFFFFF;
}

void RenderDeviceVk::createBuffer(DeviceBufferVk &deviceBuffer, size_t size, VkBufferUsageFlags usage)
{
    VkResult err;

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
    alloc_info.memoryTypeIndex = memoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
    err = vkAllocateMemory(device, &alloc_info, allocator, &deviceBuffer.bufferMemory);
    check_vk_result(err);

    err = vkBindBufferMemory(device, deviceBuffer.buffer, deviceBuffer.bufferMemory, 0);
    check_vk_result(err);
    deviceBuffer.bufferSize = size;
}

void RenderDeviceVk::uploadBuffer(const DeviceBufferVk &deviceBuffer, VkDeviceSize size, const void *data)
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
    range.size = size;
    err = vkFlushMappedMemoryRanges(device, 1, &range);
    check_vk_result(err);
    vkUnmapMemory(device, deviceBuffer.bufferMemory);
}

void RenderDeviceVk::deleteBuffer(const DeviceBufferVk &deviceBuffer)
{
    if (deviceBuffer.buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, deviceBuffer.buffer, allocator);
    if (deviceBuffer.bufferMemory)
        vkFreeMemory(device, deviceBuffer.bufferMemory, allocator);
}

void RenderDeviceVk::createBuffer(ImageBufferVk &deviceBuffer, const VkExtent3D & extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask)
{
    VkResult err;
    {
        VkImageCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = extent;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        err = vkCreateImage(device, &info, allocator, &deviceBuffer.image);
        check_vk_result(err);
    }
    {
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(device, deviceBuffer.image, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = memoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
        err = vkAllocateMemory(device, &alloc_info, allocator, &deviceBuffer.imageMemory);
        check_vk_result(err);
        err = vkBindImageMemory(device, deviceBuffer.image, deviceBuffer.imageMemory, 0);
        check_vk_result(err);
    }
    {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = deviceBuffer.image;
        view_info.format = format;
        view_info.components.r = VK_COMPONENT_SWIZZLE_R;
        view_info.components.g = VK_COMPONENT_SWIZZLE_G;
        view_info.components.b = VK_COMPONENT_SWIZZLE_B;
        view_info.components.a = VK_COMPONENT_SWIZZLE_A;
        view_info.subresourceRange.aspectMask = aspectMask;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        err = vkCreateImageView(device, &view_info, nullptr, &deviceBuffer.imageView);
        check_vk_result(err);
    }
}

void RenderDeviceVk::deleteBuffer(const ImageBufferVk &deviceBuffer)
{
    vkDestroyImageView(device, deviceBuffer.imageView, allocator);
    vkDestroyImage(device, deviceBuffer.image, allocator);
    vkFreeMemory(device, deviceBuffer.imageMemory, allocator);
}
