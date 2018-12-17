#include "DeviceVk.h"
#include "InstanceVk.h"

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

    template <typename T, size_t N> size_t countof(T (& arr)[N]) { return std::extent<T[N]>::value; }
}

using namespace Meshoui;

void DeviceVk::create(InstanceVk &instance)
{
    VkResult err;

    {
        uint32_t count;
        err = vkEnumeratePhysicalDevices(instance.instance, &count, VK_NULL_HANDLE);
        check_vk_result(err);
        std::vector<VkPhysicalDevice> gpus(count);
        err = vkEnumeratePhysicalDevices(instance.instance, &count, gpus.data());
        check_vk_result(err);
        physicalDevice = gpus[0];
    }

    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, VK_NULL_HANDLE);
        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, queues.data());
        for (uint32_t i = 0; i < count; i++)
        {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                queueFamily = i;
                break;
            }
        }
    }

    {
        uint32_t device_extensions_count = 1;
        const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = queueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.textureCompressionBC = VK_TRUE;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = countof(queue_info);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = device_extensions_count;
        create_info.ppEnabledExtensionNames = device_extensions;
        create_info.pEnabledFeatures = &deviceFeatures;
        err = vkCreateDevice(physicalDevice, &create_info, allocator, &device);
        check_vk_result(err);
        vkGetDeviceQueue(device, queueFamily, 0, &queue);
    }

    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * countof(pool_sizes);
        pool_info.poolSizeCount = countof(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(device, &pool_info, allocator, &descriptorPool);
        check_vk_result(err);
    }
}

void DeviceVk::destroy()
{
    vkDestroyDescriptorPool(device, descriptorPool, allocator);
    vkDestroyDevice(device, allocator);
}

void DeviceVk::selectSurfaceFormat(VkSurfaceKHR &surface, VkSurfaceFormatKHR &surfaceFormat, const std::vector<VkFormat> &request_formats, VkColorSpaceKHR request_color_space)
{
    surfaceFormat.format = VK_FORMAT_UNDEFINED;

    uint32_t avail_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &avail_count, VK_NULL_HANDLE);
    std::vector<VkSurfaceFormatKHR> avail_format(avail_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &avail_count, avail_format.data());

    if (avail_count == 1)
    {
        if (avail_format[0].format == VK_FORMAT_UNDEFINED)
        {
            surfaceFormat.format = request_formats[0];
            surfaceFormat.colorSpace = request_color_space;
        }
        else
        {
            surfaceFormat = avail_format[0];
        }
    }
    else
    {
        surfaceFormat = avail_format[0];
        for (auto fmt : request_formats)
        {
            for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
            {
                if (avail_format[avail_i].format == fmt && avail_format[avail_i].colorSpace == request_color_space)
                {
                    surfaceFormat = avail_format[avail_i];
                }
            }
        }
    }
}

void DeviceVk::createBuffer(DeviceBufferVk &deviceBuffer, VkDeviceSize size, VkBufferUsageFlags usage)
{
    VkResult err;

    VkDeviceSize vertex_buffer_size_aligned = ((size - 1) / memoryAlignment + 1) * memoryAlignment;
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = vertex_buffer_size_aligned;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    err = vkCreateBuffer(device, &buffer_info, allocator, &deviceBuffer.buffer);
    check_vk_result(err);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, deviceBuffer.buffer, &req);
    memoryAlignment = (memoryAlignment > req.alignment) ? memoryAlignment : req.alignment;
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = memoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
    err = vkAllocateMemory(device, &alloc_info, allocator, &deviceBuffer.memory);
    check_vk_result(err);

    err = vkBindBufferMemory(device, deviceBuffer.buffer, deviceBuffer.memory, 0);
    check_vk_result(err);
    deviceBuffer.size = size;
}

void DeviceVk::uploadBuffer(const DeviceBufferVk &deviceBuffer, VkDeviceSize size, const void *data)
{
    VkResult err;
    {
        void* dest = nullptr;
        err = vkMapMemory(device, deviceBuffer.memory, 0, size, 0, &dest);
        check_vk_result(err);
        memcpy(dest, data, size);
    }
    VkMappedMemoryRange range = {};
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.memory = deviceBuffer.memory;
    range.size = size;
    err = vkFlushMappedMemoryRanges(device, 1, &range);
    check_vk_result(err);
    vkUnmapMemory(device, deviceBuffer.memory);
}

void DeviceVk::deleteBuffer(const DeviceBufferVk &deviceBuffer)
{
    vkDestroyBuffer(device, deviceBuffer.buffer, allocator);
    vkFreeMemory(device, deviceBuffer.memory, allocator);
}

void DeviceVk::createBuffer(ImageBufferVk &deviceBuffer, const VkExtent3D & extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask)
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
        err = vkAllocateMemory(device, &alloc_info, allocator, &deviceBuffer.memory);
        check_vk_result(err);
        err = vkBindImageMemory(device, deviceBuffer.image, deviceBuffer.memory, 0);
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
        err = vkCreateImageView(device, &view_info, nullptr, &deviceBuffer.view);
        check_vk_result(err);
    }
}

void DeviceVk::transferBuffer(const DeviceBufferVk &fromBuffer, ImageBufferVk &toBuffer, const VkExtent3D & extent, VkCommandBuffer commandBuffer)
{
    {
        VkImageMemoryBarrier copy_barrier = {};
        copy_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier.image = toBuffer.image;
        copy_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_barrier.subresourceRange.levelCount = 1;
        copy_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &copy_barrier);
    }
    {
        VkBufferImageCopy region = {};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = extent;
        vkCmdCopyBufferToImage(commandBuffer, fromBuffer.buffer, toBuffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }
    {
        VkImageMemoryBarrier use_barrier = {};
        use_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        use_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        use_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        use_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        use_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        use_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        use_barrier.image = toBuffer.image;
        use_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier.subresourceRange.levelCount = 1;
        use_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &use_barrier);
    }
}

void DeviceVk::deleteBuffer(const ImageBufferVk &deviceBuffer)
{
    vkDestroyImageView(device, deviceBuffer.view, allocator);
    vkDestroyImage(device, deviceBuffer.image, allocator);
    vkFreeMemory(device, deviceBuffer.memory, allocator);
}

uint32_t DeviceVk::memoryType(VkMemoryPropertyFlags properties, uint32_t type_bits)
{
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1<<i))
            return i;
    return 0xFFFFFFFF;
}
