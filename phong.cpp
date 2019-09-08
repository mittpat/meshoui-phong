#include "phong.h"

#include <linalg.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numeric>
#include <vector>

using namespace linalg;
using namespace linalg::aliases;

static VkDebugReportCallbackEXT     g_DebugReport   = VK_NULL_HANDLE;
static MoDevice                     g_Device        = VK_NULL_HANDLE;
static MoSwapChain                  g_SwapChain     = VK_NULL_HANDLE;
static VkInstance                   g_Instance      = VK_NULL_HANDLE;
static VkPipelineCache              g_PipelineCache = VK_NULL_HANDLE;
static MoPipeline                   g_Pipeline      = VK_NULL_HANDLE;
static MoPipeline                   g_StashedPipeline = VK_NULL_HANDLE;
static uint32_t                     g_FrameIndex = 0;
static const VkAllocationCallbacks* g_Allocator     = VK_NULL_HANDLE;

template <typename T, size_t N> size_t countof(T (& arr)[N]) { return std::extent<T[N]>::value; }

static uint32_t memoryType(VkPhysicalDevice physicalDevice, VkMemoryPropertyFlags properties, uint32_t type_bits)
{
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1<<i))
            return i;
    return 0xFFFFFFFF;
}

static void createBuffer(MoDevice device, MoDeviceBuffer *pDeviceBuffer, VkDeviceSize size, VkBufferUsageFlags usage)
{
    MoDeviceBuffer deviceBuffer = *pDeviceBuffer = new MoDeviceBuffer_T();
    *deviceBuffer = {};

    VkResult err;
    {
        VkDeviceSize buffer_size_aligned = ((size - 1) / device->memoryAlignment + 1) * device->memoryAlignment;
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = buffer_size_aligned;
        buffer_info.usage = usage;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        err = vkCreateBuffer(device->device, &buffer_info, g_Allocator, &deviceBuffer->buffer);
        device->pCheckVkResultFn(err);
    }
    {
        VkMemoryRequirements req;
        vkGetBufferMemoryRequirements(device->device, deviceBuffer->buffer, &req);
        device->memoryAlignment = (device->memoryAlignment > req.alignment) ? device->memoryAlignment : req.alignment;
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = memoryType(device->physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
        err = vkAllocateMemory(device->device, &alloc_info, g_Allocator, &deviceBuffer->memory);
        device->pCheckVkResultFn(err);
    }

    err = vkBindBufferMemory(device->device, deviceBuffer->buffer, deviceBuffer->memory, 0);
    device->pCheckVkResultFn(err);
    deviceBuffer->size = size;
}

static void uploadBuffer(MoDevice device, MoDeviceBuffer deviceBuffer, VkDeviceSize dataSize, const void *pData)
{
    VkResult err;
    {
        void* dest = nullptr;
        err = vkMapMemory(device->device, deviceBuffer->memory, 0, dataSize, 0, &dest);
        device->pCheckVkResultFn(err);
        memcpy(dest, pData, dataSize);
    }
    {
        VkMappedMemoryRange range = {};
        range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range.memory = deviceBuffer->memory;
        range.size = VK_WHOLE_SIZE;
        err = vkFlushMappedMemoryRanges(device->device, 1, &range);
        device->pCheckVkResultFn(err);
    }

    vkUnmapMemory(device->device, deviceBuffer->memory);
}

static void deleteBuffer(MoDevice device, MoDeviceBuffer deviceBuffer)
{
    vkDestroyBuffer(device->device, deviceBuffer->buffer, g_Allocator);
    vkFreeMemory(device->device, deviceBuffer->memory, g_Allocator);
    delete deviceBuffer;
}

static void createBuffer(MoDevice device, MoImageBuffer *pImageBuffer, const VkExtent3D & extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspectMask)
{
    MoImageBuffer imageBuffer = *pImageBuffer = new MoImageBuffer_T();
    *imageBuffer = {};

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
        err = vkCreateImage(device->device, &info, g_Allocator, &imageBuffer->image);
        device->pCheckVkResultFn(err);
    }
    {
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(device->device, imageBuffer->image, &req);
        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = req.size;
        alloc_info.memoryTypeIndex = memoryType(device->physicalDevice, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
        err = vkAllocateMemory(device->device, &alloc_info, g_Allocator, &imageBuffer->memory);
        device->pCheckVkResultFn(err);
        err = vkBindImageMemory(device->device, imageBuffer->image, imageBuffer->memory, 0);
        device->pCheckVkResultFn(err);
    }
    {
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = imageBuffer->image;
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
        err = vkCreateImageView(device->device, &view_info, nullptr, &imageBuffer->view);
        device->pCheckVkResultFn(err);
    }
}

static void transferBuffer(VkCommandBuffer commandBuffer, MoDeviceBuffer fromBuffer, MoImageBuffer toBuffer, const VkExtent3D & extent)
{
    {
        VkImageMemoryBarrier copy_barrier = {};
        copy_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        copy_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        copy_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        copy_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        copy_barrier.image = toBuffer->image;
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
        vkCmdCopyBufferToImage(commandBuffer, fromBuffer->buffer, toBuffer->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
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
        use_barrier.image = toBuffer->image;
        use_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        use_barrier.subresourceRange.levelCount = 1;
        use_barrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &use_barrier);
    }
}

static void deleteBuffer(MoDevice device, MoImageBuffer imageBuffer)
{
    vkDestroyImageView(device->device, imageBuffer->view, g_Allocator);
    vkDestroyImage(device->device, imageBuffer->image, g_Allocator);
    vkFreeMemory(device->device, imageBuffer->memory, g_Allocator);
    delete imageBuffer;
}

static void generateTexture(MoImageBuffer *pImageBuffer, const MoTextureInfo &textureInfo, const float4 &fallbackColor, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
    VkFormat format = textureInfo.format == VK_FORMAT_UNDEFINED ? VK_FORMAT_R8G8B8A8_UNORM : textureInfo.format;
    unsigned width = textureInfo.extent.width, height = textureInfo.extent.height;
    std::vector<uint8_t> data;
    const uint8_t* dataPtr = textureInfo.pData;
    if (dataPtr == nullptr)
    {
        // use fallback
        width = height = 1;
        data.resize(4);
        data[0] = (uint8_t)(fallbackColor.x * 0xFF);
        data[1] = (uint8_t)(fallbackColor.y * 0xFF);
        data[2] = (uint8_t)(fallbackColor.z * 0xFF);
        data[3] = (uint8_t)(fallbackColor.w * 0xFF);
        dataPtr = data.data();
    }
    VkDeviceSize size = textureInfo.dataSize;
    if (size == 0)
    {
        size = width * height * 4;
        switch (format)
        {
        case VK_FORMAT_R8G8B8A8_UNORM:
            break;
        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
            size /= 6;
            break;
        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
            size /= 4;
            break;
        }
    }

    VkResult err;

    // begin
    {
        err = vkResetCommandPool(g_Device->device, commandPool, 0);
        g_Device->pCheckVkResultFn(err);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(commandBuffer, &begin_info);
        g_Device->pCheckVkResultFn(err);
    }

    // create buffer
    createBuffer(g_Device, pImageBuffer, {width, height, 1}, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    // upload
    MoDeviceBuffer upload = {};
    createBuffer(g_Device, &upload, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    uploadBuffer(g_Device, upload, size, dataPtr);
    transferBuffer(commandBuffer, upload, *pImageBuffer, {width, height, 1});

    // end
    {
        VkSubmitInfo endInfo = {};
        endInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        endInfo.commandBufferCount = 1;
        endInfo.pCommandBuffers = &commandBuffer;
        err = vkEndCommandBuffer(commandBuffer);
        g_Device->pCheckVkResultFn(err);
        err = vkQueueSubmit(g_Device->queue, 1, &endInfo, VK_NULL_HANDLE);
        g_Device->pCheckVkResultFn(err);
    }

    // wait
    err = vkDeviceWaitIdle(g_Device->device);
    g_Device->pCheckVkResultFn(err);

    deleteBuffer(g_Device, upload);
}

void moCreateInstance(MoInstanceCreateInfo *pCreateInfo, VkInstance *pInstance)
{
    VkResult err;

    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.enabledExtensionCount = pCreateInfo->extensionsCount;
        create_info.ppEnabledExtensionNames = pCreateInfo->pExtensions;
        if (pCreateInfo->debugReport)
        {
            // Enabling multiple validation layers grouped as LunarG standard validation
            const char* layers[] = { "VK_LAYER_LUNARG_standard_validation" };
            create_info.enabledLayerCount = 1;
            create_info.ppEnabledLayerNames = layers;

            // Enable debug report extension (we need additional storage, so we duplicate the user array to add our new extension to it)
            const char** extensions_ext = (const char**)malloc(sizeof(const char*) * (pCreateInfo->extensionsCount + 1));
            memcpy(extensions_ext, pCreateInfo->pExtensions, pCreateInfo->extensionsCount * sizeof(const char*));
            extensions_ext[pCreateInfo->extensionsCount] = "VK_EXT_debug_report";
            create_info.enabledExtensionCount = pCreateInfo->extensionsCount + 1;
            create_info.ppEnabledExtensionNames = extensions_ext;

            // Create Vulkan Instance
            err = vkCreateInstance(&create_info, pCreateInfo->pAllocator, pInstance);
            pCreateInfo->pCheckVkResultFn(err);
            free(extensions_ext);

            // Get the function pointer (required for any extensions)
            auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(*pInstance, "vkCreateDebugReportCallbackEXT");

            // Setup the debug report callback
            VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
            debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debug_report_ci.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_DEBUG_BIT_EXT;
            debug_report_ci.pfnCallback = pCreateInfo->pDebugReportCallback;
            debug_report_ci.pUserData = nullptr;
            err = vkCreateDebugReportCallbackEXT(*pInstance, &debug_report_ci, pCreateInfo->pAllocator, &g_DebugReport);
            pCreateInfo->pCheckVkResultFn(err);
        }
        else
        {
            err = vkCreateInstance(&create_info, pCreateInfo->pAllocator, pInstance);
            pCreateInfo->pCheckVkResultFn(err);
        }
    }
}

void moDestroyInstance(VkInstance instance)
{
    if (g_DebugReport)
    {
        auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        vkDestroyDebugReportCallbackEXT(instance, g_DebugReport, g_Allocator);
    }
    else
    {
        vkDestroyInstance(instance, g_Allocator);
    }
}

void moCreateDevice(MoDeviceCreateInfo *pCreateInfo, MoDevice *pDevice)
{
    MoDevice device = *pDevice = new MoDevice_T();
    *device = {};

    VkResult err;

    {
        uint32_t count;
        err = vkEnumeratePhysicalDevices(pCreateInfo->instance, &count, VK_NULL_HANDLE);
        pCreateInfo->pCheckVkResultFn(err);
        std::vector<VkPhysicalDevice> gpus(count);
        err = vkEnumeratePhysicalDevices(pCreateInfo->instance, &count, gpus.data());
        pCreateInfo->pCheckVkResultFn(err);
        device->physicalDevice = gpus[0];
    }

    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(device->physicalDevice, &count, VK_NULL_HANDLE);
        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device->physicalDevice, &count, queues.data());
        for (uint32_t i = 0; i < count; i++)
        {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                device->queueFamily = i;
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
        queue_info[0].queueFamilyIndex = device->queueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.textureCompressionBC = VK_TRUE;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = (uint32_t)countof(queue_info);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = device_extensions_count;
        create_info.ppEnabledExtensionNames = device_extensions;
        create_info.pEnabledFeatures = &deviceFeatures;
        err = vkCreateDevice(device->physicalDevice, &create_info, g_Allocator, &device->device);
        pCreateInfo->pCheckVkResultFn(err);
        vkGetDeviceQueue(device->device, device->queueFamily, 0, &device->queue);
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
        pool_info.maxSets = 1000 * (uint32_t)countof(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)countof(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(device->device, &pool_info, g_Allocator, &device->descriptorPool);
        pCreateInfo->pCheckVkResultFn(err);
    }

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(device->physicalDevice, device->queueFamily, pCreateInfo->surface, &res);
    if (res != VK_TRUE)
    {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        abort();
    }

    pCreateInfo->pSurfaceFormat->format = VK_FORMAT_UNDEFINED;

    {
        uint32_t avail_count;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, pCreateInfo->surface, &avail_count, VK_NULL_HANDLE);
        std::vector<VkSurfaceFormatKHR> avail_format(avail_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device->physicalDevice, pCreateInfo->surface, &avail_count, avail_format.data());

        if (avail_count == 1)
        {
            if (avail_format[0].format == VK_FORMAT_UNDEFINED)
            {
                pCreateInfo->pSurfaceFormat->format = pCreateInfo->pRequestFormats[0];
                pCreateInfo->pSurfaceFormat->colorSpace = pCreateInfo->requestColorSpace;
            }
            else
            {
                *pCreateInfo->pSurfaceFormat = avail_format[0];
            }
        }
        else
        {
            *pCreateInfo->pSurfaceFormat = avail_format[0];
            for (uint32_t i = 0; i < pCreateInfo->requestFormatsCount; ++i)
            {
                for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
                {
                    if (avail_format[avail_i].format == pCreateInfo->pRequestFormats[i] && avail_format[avail_i].colorSpace == pCreateInfo->requestColorSpace)
                    {
                        *pCreateInfo->pSurfaceFormat = avail_format[avail_i];
                    }
                }
            }
        }
    }

    device->pCheckVkResultFn = pCreateInfo->pCheckVkResultFn;
}

void moDestroyDevice(MoDevice device)
{
    vkDestroyDescriptorPool(device->device, device->descriptorPool, g_Allocator);
    vkDestroyDevice(device->device, g_Allocator);
    delete device;
}

void moCreateSwapChain(MoSwapChainCreateInfo *pCreateInfo, MoSwapChain *pSwapChain)
{
    MoSwapChain swapChain = *pSwapChain = new MoSwapChain_T();
    *swapChain = {};

    VkResult err;
    // Create command buffers
    for (uint32_t i = 0; i < countof(swapChain->frames); ++i)
    {
        {
            VkCommandPoolCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            info.queueFamilyIndex = pCreateInfo->device->queueFamily;
            err = vkCreateCommandPool(pCreateInfo->device->device, &info, pCreateInfo->pAllocator, &swapChain->frames[i].pool);
            pCreateInfo->pCheckVkResultFn(err);
        }
        {
            VkCommandBufferAllocateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            info.commandPool = swapChain->frames[i].pool;
            info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            info.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(pCreateInfo->device->device, &info, &swapChain->frames[i].buffer);
            pCreateInfo->pCheckVkResultFn(err);
        }
        {
            VkFenceCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            err = vkCreateFence(pCreateInfo->device->device, &info, pCreateInfo->pAllocator, &swapChain->frames[i].fence);
            pCreateInfo->pCheckVkResultFn(err);
        }
        {
            VkSemaphoreCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            err = vkCreateSemaphore(pCreateInfo->device->device, &info, pCreateInfo->pAllocator, &swapChain->frames[i].acquired);
            pCreateInfo->pCheckVkResultFn(err);
            err = vkCreateSemaphore(pCreateInfo->device->device, &info, pCreateInfo->pAllocator, &swapChain->frames[i].complete);
            pCreateInfo->pCheckVkResultFn(err);
        }
    }

    // Create image buffers
    {
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = pCreateInfo->surface;
        info.minImageCount = MO_FRAME_COUNT;
        info.imageFormat = pCreateInfo->surfaceFormat.format;
        info.imageColorSpace = pCreateInfo->surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
        info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = pCreateInfo->vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = VK_NULL_HANDLE;
        VkSurfaceCapabilitiesKHR cap;
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pCreateInfo->device->physicalDevice, pCreateInfo->surface, &cap);
        pCreateInfo->pCheckVkResultFn(err);
        if (info.minImageCount < cap.minImageCount)
            info.minImageCount = cap.minImageCount;
        else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
            info.minImageCount = cap.maxImageCount;

        if (cap.currentExtent.width == 0xffffffff)
        {
            info.imageExtent = swapChain->extent = pCreateInfo->extent;
        }
        else
        {
            info.imageExtent = swapChain->extent = cap.currentExtent;
        }
        err = vkCreateSwapchainKHR(pCreateInfo->device->device, &info, pCreateInfo->pAllocator, &swapChain->swapChainKHR);
        pCreateInfo->pCheckVkResultFn(err);
        uint32_t backBufferCount = 0;
        err = vkGetSwapchainImagesKHR(pCreateInfo->device->device, swapChain->swapChainKHR, &backBufferCount, NULL);
        pCreateInfo->pCheckVkResultFn(err);
        VkImage backBuffer[MO_FRAME_COUNT] = {};
        err = vkGetSwapchainImagesKHR(pCreateInfo->device->device, swapChain->swapChainKHR, &backBufferCount, backBuffer);
        pCreateInfo->pCheckVkResultFn(err);

        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            swapChain->images[i].back = backBuffer[i];
        }
    }

    {
        VkAttachmentDescription attachment[2] = {};
        attachment[0].format = pCreateInfo->surfaceFormat.format;
        attachment[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment[1].format = VK_FORMAT_D16_UNORM;
        attachment[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depth_attachment = {};
        depth_attachment.attachment = 1;
        depth_attachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        subpass.pDepthStencilAttachment = &depth_attachment;

        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        err = vkCreateRenderPass(pCreateInfo->device->device, &info, pCreateInfo->pAllocator, &swapChain->renderPass);
        pCreateInfo->pCheckVkResultFn(err);
    }
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = pCreateInfo->surfaceFormat.format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        info.subresourceRange = image_range;
        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            info.image = swapChain->images[i].back;
            err = vkCreateImageView(pCreateInfo->device->device, &info, pCreateInfo->pAllocator, &swapChain->images[i].view);
            pCreateInfo->pCheckVkResultFn(err);
        }
    }

    // depth buffer
    createBuffer(pCreateInfo->device, &swapChain->depthBuffer, {swapChain->extent.width, swapChain->extent.height, 1}, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    {
        VkImageView attachment[2] = {0, swapChain->depthBuffer->view};
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = swapChain->renderPass;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.width = swapChain->extent.width;
        info.height = swapChain->extent.height;
        info.layers = 1;
        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            attachment[0] = swapChain->images[i].view;
            err = vkCreateFramebuffer(pCreateInfo->device->device, &info, pCreateInfo->pAllocator, &swapChain->images[i].front);
            pCreateInfo->pCheckVkResultFn(err);
        }
    }

    swapChain->clearColor = pCreateInfo->clearColor;
}

void moRecreateSwapChain(MoSwapChainRecreateInfo *pCreateInfo, MoSwapChain swapChain)
{
    VkResult err;
    VkSwapchainKHR old_swapchain = swapChain->swapChainKHR;
    err = vkDeviceWaitIdle(g_Device->device);
    g_Device->pCheckVkResultFn(err);

    deleteBuffer(g_Device, swapChain->depthBuffer);
    for (uint32_t i = 0; i < countof(swapChain->images); ++i)
    {
        vkDestroyImageView(g_Device->device, swapChain->images[i].view, g_Allocator);
        vkDestroyFramebuffer(g_Device->device, swapChain->images[i].front, g_Allocator);
    }
    if (swapChain->renderPass)
    {
        vkDestroyRenderPass(g_Device->device, swapChain->renderPass, g_Allocator);
    }

    {
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = pCreateInfo->surface;
        info.minImageCount = MO_FRAME_COUNT;
        info.imageFormat = pCreateInfo->surfaceFormat.format;
        info.imageColorSpace = pCreateInfo->surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
        info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = pCreateInfo->vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = old_swapchain;
        VkSurfaceCapabilitiesKHR cap;
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_Device->physicalDevice, pCreateInfo->surface, &cap);
        g_Device->pCheckVkResultFn(err);
        if (info.minImageCount < cap.minImageCount)
            info.minImageCount = cap.minImageCount;
        else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
            info.minImageCount = cap.maxImageCount;

        if (cap.currentExtent.width == 0xffffffff)
        {
            info.imageExtent = swapChain->extent = pCreateInfo->extent;
        }
        else
        {
            info.imageExtent = swapChain->extent = cap.currentExtent;
        }
        err = vkCreateSwapchainKHR(g_Device->device, &info, g_Allocator, &swapChain->swapChainKHR);
        g_Device->pCheckVkResultFn(err);
        uint32_t backBufferCount = 0;
        err = vkGetSwapchainImagesKHR(g_Device->device, swapChain->swapChainKHR, &backBufferCount, NULL);
        g_Device->pCheckVkResultFn(err);
        VkImage backBuffer[MO_FRAME_COUNT] = {};
        err = vkGetSwapchainImagesKHR(g_Device->device, swapChain->swapChainKHR, &backBufferCount, backBuffer);
        g_Device->pCheckVkResultFn(err);

        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            swapChain->images[i].back = backBuffer[i];
        }
    }
    if (old_swapchain)
    {
        vkDestroySwapchainKHR(g_Device->device, old_swapchain, g_Allocator);
    }

    {
        VkAttachmentDescription attachment[2] = {};
        attachment[0].format = pCreateInfo->surfaceFormat.format;
        attachment[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment[1].format = VK_FORMAT_D16_UNORM;
        attachment[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkAttachmentReference depth_attachment = {};
        depth_attachment.attachment = 1;
        depth_attachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        subpass.pDepthStencilAttachment = &depth_attachment;

        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        err = vkCreateRenderPass(g_Device->device, &info, g_Allocator, &swapChain->renderPass);
        g_Device->pCheckVkResultFn(err);
    }
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = pCreateInfo->surfaceFormat.format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        info.subresourceRange = image_range;
        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            info.image = swapChain->images[i].back;
            err = vkCreateImageView(g_Device->device, &info, g_Allocator, &swapChain->images[i].view);
            g_Device->pCheckVkResultFn(err);
        }
    }

    // depth buffer
    createBuffer(g_Device, &swapChain->depthBuffer, { swapChain->extent.width, swapChain->extent.height, 1}, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    {
        VkImageView attachment[2] = {0, swapChain->depthBuffer->view};
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = swapChain->renderPass;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.width = swapChain->extent.width;
        info.height = swapChain->extent.height;
        info.layers = 1;
        for (uint32_t i = 0; i < countof(swapChain->images); ++i)
        {
            attachment[0] = swapChain->images[i].view;
            err = vkCreateFramebuffer(g_Device->device, &info, g_Allocator, &swapChain->images[i].front);
            g_Device->pCheckVkResultFn(err);
        }
    }
}

void moBeginSwapChain(MoSwapChain swapChain, uint32_t *pFrameIndex, VkSemaphore *pImageAcquiredSemaphore)
{
    VkResult err;

    *pImageAcquiredSemaphore = swapChain->frames[*pFrameIndex].acquired;
    {
        err = vkAcquireNextImageKHR(g_Device->device, swapChain->swapChainKHR, UINT64_MAX, *pImageAcquiredSemaphore, VK_NULL_HANDLE, pFrameIndex);
        g_Device->pCheckVkResultFn(err);

        err = vkWaitForFences(g_Device->device, 1, &swapChain->frames[*pFrameIndex].fence, VK_TRUE, UINT64_MAX);    // wait indefinitely instead of periodically checking
        g_Device->pCheckVkResultFn(err);

        err = vkResetFences(g_Device->device, 1, &swapChain->frames[*pFrameIndex].fence);
        g_Device->pCheckVkResultFn(err);
    }
    {
        err = vkResetCommandPool(g_Device->device, swapChain->frames[*pFrameIndex].pool, 0);
        g_Device->pCheckVkResultFn(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(swapChain->frames[*pFrameIndex].buffer, &info);
        g_Device->pCheckVkResultFn(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = swapChain->renderPass;
        info.framebuffer = swapChain->images[*pFrameIndex].front;
        info.renderArea.extent = swapChain->extent;
        VkClearValue clearValue[2] = {};
        clearValue[0].color = {{swapChain->clearColor.x, swapChain->clearColor.y, swapChain->clearColor.z, swapChain->clearColor.w}};
        clearValue[1].depthStencil = {1.0f, 0};
        info.pClearValues = clearValue;
        info.clearValueCount = 2;
        vkCmdBeginRenderPass(swapChain->frames[*pFrameIndex].buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    VkViewport viewport{ 0, 0, float(swapChain->extent.width), float(swapChain->extent.height), 0.f, 1.f };
    vkCmdSetViewport(swapChain->frames[*pFrameIndex].buffer, 0, 1, &viewport);
    VkRect2D scissor{ { 0, 0 },{ swapChain->extent.width, swapChain->extent.height } };
    vkCmdSetScissor(swapChain->frames[*pFrameIndex].buffer, 0, 1, &scissor);
}

VkResult moEndSwapChain(MoSwapChain swapChain, uint32_t *pFrameIndex, VkSemaphore *pImageAcquiredSemaphore)
{
    vkCmdEndRenderPass(swapChain->frames[*pFrameIndex].buffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = pImageAcquiredSemaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &swapChain->frames[*pFrameIndex].buffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &swapChain->frames[*pFrameIndex].complete;

        VkResult err = vkEndCommandBuffer(swapChain->frames[*pFrameIndex].buffer);
        g_Device->pCheckVkResultFn(err);
        err = vkQueueSubmit(g_Device->queue, 1, &info, swapChain->frames[*pFrameIndex].fence);
        g_Device->pCheckVkResultFn(err);
    }

    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &swapChain->frames[*pFrameIndex].complete;
    info.swapchainCount = 1;
    info.pSwapchains = &swapChain->swapChainKHR;
    info.pImageIndices = pFrameIndex;
    return vkQueuePresentKHR(g_Device->queue, &info);
}

void moDestroySwapChain(MoDevice device, MoSwapChain pSwapChain)
{
    VkResult err;
    err = vkDeviceWaitIdle(device->device);
    device->pCheckVkResultFn(err);
    vkQueueWaitIdle(device->queue);
    for (uint32_t i = 0; i < countof(pSwapChain->frames); ++i)
    {
        vkDestroyFence(device->device, pSwapChain->frames[i].fence, g_Allocator);
        vkFreeCommandBuffers(device->device, pSwapChain->frames[i].pool, 1, &pSwapChain->frames[i].buffer);
        vkDestroyCommandPool(device->device, pSwapChain->frames[i].pool, g_Allocator);
        vkDestroySemaphore(device->device, pSwapChain->frames[i].acquired, g_Allocator);
        vkDestroySemaphore(device->device, pSwapChain->frames[i].complete, g_Allocator);
    }

    deleteBuffer(device, pSwapChain->depthBuffer);
    for (uint32_t i = 0; i < countof(pSwapChain->images); ++i)
    {
        vkDestroyImageView(device->device, pSwapChain->images[i].view, g_Allocator);
        vkDestroyFramebuffer(device->device, pSwapChain->images[i].front, g_Allocator);
    }
    vkDestroyRenderPass(device->device, pSwapChain->renderPass, g_Allocator);
    vkDestroySwapchainKHR(device->device, pSwapChain->swapChainKHR, g_Allocator);
}

void moInit(MoInitInfo *pInfo)
{
    assert(g_Instance == VK_NULL_HANDLE);
    assert(g_Device == VK_NULL_HANDLE);
    assert(g_PipelineCache == VK_NULL_HANDLE);
    assert(g_Allocator == VK_NULL_HANDLE);

    g_Instance = pInfo->instance;
    g_Device = new MoDevice_T;
    *g_Device = {};
    g_Device->memoryAlignment = 256;
    g_Device->physicalDevice = pInfo->physicalDevice;
    g_Device->device = pInfo->device;
    g_Device->queueFamily = pInfo->queueFamily;
    g_Device->queue = pInfo->queue;
    g_Device->pCheckVkResultFn = pInfo->pCheckVkResultFn;
    g_PipelineCache = pInfo->pipelineCache;
    g_Device->descriptorPool = pInfo->descriptorPool;
    g_SwapChain = new MoSwapChain_T;
    *g_SwapChain = {};
    g_SwapChain->depthBuffer = pInfo->depthBuffer;
    g_SwapChain->swapChainKHR = pInfo->swapChainKHR;
    g_SwapChain->renderPass = pInfo->renderPass;
    g_SwapChain->extent = pInfo->extent;
    for (uint32_t i = 0; i < pInfo->swapChainCommandBufferCount; ++i)
    {
        g_SwapChain->frames[i].pool = pInfo->pSwapChainCommandBuffers[i].pool;
        g_SwapChain->frames[i].buffer = pInfo->pSwapChainCommandBuffers[i].buffer;
        g_SwapChain->frames[i].fence = pInfo->pSwapChainCommandBuffers[i].fence;
        g_SwapChain->frames[i].acquired = pInfo->pSwapChainCommandBuffers[i].acquired;
        g_SwapChain->frames[i].complete = pInfo->pSwapChainCommandBuffers[i].complete;
    }
    for (uint32_t i = 0; i < pInfo->swapChainSwapBufferCount; ++i)
    {
        g_SwapChain->images[i].back = pInfo->pSwapChainSwapBuffers[i].back;
        g_SwapChain->images[i].view = pInfo->pSwapChainSwapBuffers[i].view;
        g_SwapChain->images[i].front = pInfo->pSwapChainSwapBuffers[i].front;
    }
    g_Allocator = pInfo->pAllocator;

    MoPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.flags = MO_PIPELINE_FEATURE_DEFAULT;
    std::vector<char> mo_phong_shader_vert_spv;
    {
        std::ifstream fileStream("phong.vert.spv", std::ifstream::binary);
        mo_phong_shader_vert_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    std::vector<char> mo_phong_shader_frag_spv;
    {
        std::ifstream fileStream("phong.frag.spv", std::ifstream::binary);
        mo_phong_shader_frag_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    pipelineCreateInfo.pVertexShader = (std::uint32_t*)mo_phong_shader_vert_spv.data();
    pipelineCreateInfo.vertexShaderSize = mo_phong_shader_vert_spv.size();
    pipelineCreateInfo.pFragmentShader = (std::uint32_t*)mo_phong_shader_frag_spv.data();
    pipelineCreateInfo.fragmentShaderSize = mo_phong_shader_frag_spv.size();

    moCreatePipeline(&pipelineCreateInfo, &g_Pipeline);
}

void moShutdown()
{
    moDestroyPipeline(g_Pipeline);
    g_Pipeline = VK_NULL_HANDLE;
    g_Instance = VK_NULL_HANDLE;
    g_Device->physicalDevice = VK_NULL_HANDLE;
    g_Device->device = VK_NULL_HANDLE;
    g_Device->queueFamily = -1;
    g_Device->queue = VK_NULL_HANDLE;
    g_Device->memoryAlignment = 256;
    g_SwapChain->swapChainKHR = VK_NULL_HANDLE;
    g_SwapChain->renderPass = VK_NULL_HANDLE;
    g_SwapChain->extent = {0, 0};
//    g_SwapChain->frames = {};
//    g_SwapChain->images = {};
    g_PipelineCache = VK_NULL_HANDLE;
    g_Device->descriptorPool = VK_NULL_HANDLE;
    delete g_Device;
    delete g_SwapChain;
    g_Device = VK_NULL_HANDLE;
    g_SwapChain = VK_NULL_HANDLE;
    g_Allocator = VK_NULL_HANDLE;
}

void moCreatePipeline(const MoPipelineCreateInfo *pCreateInfo, MoPipeline *pPipeline)
{
    MoPipeline pipeline = *pPipeline = new MoPipeline_T();
    *pipeline = {};

    VkResult err;
    VkShaderModule vert_module;
    VkShaderModule frag_module;

    {
        VkShaderModuleCreateInfo vert_info = {};
        vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vert_info.codeSize = pCreateInfo->vertexShaderSize;
        vert_info.pCode = pCreateInfo->pVertexShader;
        err = vkCreateShaderModule(g_Device->device, &vert_info, g_Allocator, &vert_module);
        g_Device->pCheckVkResultFn(err);
        VkShaderModuleCreateInfo frag_info = {};
        frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        frag_info.codeSize = pCreateInfo->fragmentShaderSize;
        frag_info.pCode = pCreateInfo->pFragmentShader;
        err = vkCreateShaderModule(g_Device->device, &frag_info, g_Allocator, &frag_module);
        g_Device->pCheckVkResultFn(err);
    }

    {
        VkDescriptorSetLayoutBinding binding[2];
        binding[0].binding = 0;
        binding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding[0].descriptorCount = 1;
        binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
        binding[1].binding = 1;
        binding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding[1].descriptorCount = 1;
        binding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = (uint32_t)countof(binding);
        info.pBindings = binding;
        err = vkCreateDescriptorSetLayout(g_Device->device, &info, g_Allocator, &pipeline->descriptorSetLayout[MO_PROGRAM_DESC_LAYOUT]);
        g_Device->pCheckVkResultFn(err);
    }

    {
        VkDescriptorSetLayoutBinding binding[5];
        for (uint32_t i = 0; i < 5; ++i)
        {
            binding[i].binding = i;
            binding[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding[i].descriptorCount = 1;
            binding[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding[i].pImmutableSamplers = VK_NULL_HANDLE;
        }
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = (uint32_t)countof(binding);
        info.pBindings = binding;
        err = vkCreateDescriptorSetLayout(g_Device->device, &info, g_Allocator, &pipeline->descriptorSetLayout[MO_MATERIAL_DESC_LAYOUT]);
        g_Device->pCheckVkResultFn(err);
    }

    {
        VkDescriptorSetLayout descriptorSetLayout[MO_FRAME_COUNT] = {};
        for (size_t i = 0; i < MO_FRAME_COUNT; ++i)
            descriptorSetLayout[i] = pipeline->descriptorSetLayout[MO_PROGRAM_DESC_LAYOUT];
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = g_Device->descriptorPool;
        alloc_info.descriptorSetCount = MO_FRAME_COUNT;
        alloc_info.pSetLayouts = descriptorSetLayout;
        err = vkAllocateDescriptorSets(g_Device->device, &alloc_info, pipeline->descriptorSet);
        g_Device->pCheckVkResultFn(err);
    }

    for (size_t i = 0; i < MO_FRAME_COUNT; ++i)
    {
        createBuffer(g_Device, &pipeline->uniformBuffer[i], sizeof(MoUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        VkDescriptorBufferInfo bufferInfo[1] = {};
        bufferInfo[0].buffer = pipeline->uniformBuffer[i]->buffer;
        bufferInfo[0].offset = 0;
        bufferInfo[0].range = sizeof(MoUniform);

        VkWriteDescriptorSet descriptorWrite[1] = {};
        descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite[0].dstSet = pipeline->descriptorSet[i];
        descriptorWrite[0].dstBinding = 0;
        descriptorWrite[0].dstArrayElement = 0;
        descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite[0].descriptorCount = 1;
        descriptorWrite[0].pBufferInfo = &bufferInfo[0];

        vkUpdateDescriptorSets(g_Device->device, 1, descriptorWrite, 0, nullptr);
    }

    {
        // model, view & projection
        std::vector<VkPushConstantRange> push_constants;
        push_constants.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MoPushConstant)});
        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = (uint32_t)countof(pipeline->descriptorSetLayout);
        layout_info.pSetLayouts = pipeline->descriptorSetLayout;
        layout_info.pushConstantRangeCount = (uint32_t)push_constants.size();
        layout_info.pPushConstantRanges = push_constants.data();
        err = vkCreatePipelineLayout(g_Device->device, &layout_info, g_Allocator, &pipeline->pipelineLayout);
        g_Device->pCheckVkResultFn(err);
    }

    VkPipelineShaderStageCreateInfo stage[2] = {};
    stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage[0].module = vert_module;
    stage[0].pName = "main";
    stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage[1].module = frag_module;
    stage[1].pName = "main";

    VkVertexInputBindingDescription binding_desc[5] = {};
    binding_desc[0].binding = 0;
    binding_desc[0].stride = sizeof(float3);
    binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_desc[1].binding = 1;
    binding_desc[1].stride = sizeof(float2);
    binding_desc[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_desc[2].binding = 2;
    binding_desc[2].stride = sizeof(float3);
    binding_desc[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_desc[3].binding = 3;
    binding_desc[3].stride = sizeof(float3);
    binding_desc[3].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    binding_desc[4].binding = 4;
    binding_desc[4].stride = sizeof(float3);
    binding_desc[4].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attribute_desc;
    attribute_desc.emplace_back(VkVertexInputAttributeDescription{0, binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, 0 });
    attribute_desc.emplace_back(VkVertexInputAttributeDescription{1, binding_desc[1].binding, VK_FORMAT_R32G32_SFLOAT,    0 });
    attribute_desc.emplace_back(VkVertexInputAttributeDescription{2, binding_desc[2].binding, VK_FORMAT_R32G32B32_SFLOAT, 0 });
    attribute_desc.emplace_back(VkVertexInputAttributeDescription{3, binding_desc[3].binding, VK_FORMAT_R32G32B32_SFLOAT, 0 });
    attribute_desc.emplace_back(VkVertexInputAttributeDescription{4, binding_desc[4].binding, VK_FORMAT_R32G32B32_SFLOAT, 0 });

    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_info.vertexBindingDescriptionCount = (uint32_t)countof(binding_desc);
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = (uint32_t)attribute_desc.size();
    vertex_info.pVertexAttributeDescriptions = attribute_desc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.cullMode = pCreateInfo->flags & MO_PIPELINE_FEATURE_BACKFACE_CULLING ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_info = {};
    ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_attachment[1] = {};
    color_attachment[0].blendEnable = VK_TRUE;
    color_attachment[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment[0].alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_info.depthTestEnable = pCreateInfo->flags & MO_PIPELINE_FEATURE_DEPTH_TEST ? VK_TRUE : VK_FALSE;
    depth_info.depthWriteEnable = pCreateInfo->flags & MO_PIPELINE_FEATURE_DEPTH_WRITE ? VK_TRUE : VK_FALSE;
    depth_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_info.depthBoundsTestEnable = VK_FALSE;
    depth_info.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = (uint32_t)countof(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.flags = 0;
    info.stageCount = 2;
    info.pStages = stage;
    info.pVertexInputState = &vertex_info;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewport_info;
    info.pRasterizationState = &raster_info;
    info.pMultisampleState = &ms_info;
    info.pDepthStencilState = &depth_info;
    info.pColorBlendState = &blend_info;
    info.pDynamicState = &dynamic_state;
    info.layout = pipeline->pipelineLayout;
    info.renderPass = g_SwapChain->renderPass;
    err = vkCreateGraphicsPipelines(g_Device->device, g_PipelineCache, 1, &info, g_Allocator, &pipeline->pipeline);
    g_Device->pCheckVkResultFn(err);

    vkDestroyShaderModule(g_Device->device, frag_module, nullptr);
    vkDestroyShaderModule(g_Device->device, vert_module, nullptr);
}

void moDestroyPipeline(MoPipeline pipeline)
{
    vkQueueWaitIdle(g_Device->queue);
    for (size_t i = 0; i < MO_FRAME_COUNT; ++i) { deleteBuffer(g_Device, pipeline->uniformBuffer[i]); }
    vkDestroyDescriptorSetLayout(g_Device->device, pipeline->descriptorSetLayout[MO_PROGRAM_DESC_LAYOUT], g_Allocator);
    vkDestroyDescriptorSetLayout(g_Device->device, pipeline->descriptorSetLayout[MO_MATERIAL_DESC_LAYOUT], g_Allocator);
    vkDestroyPipelineLayout(g_Device->device, pipeline->pipelineLayout, g_Allocator);
    vkDestroyPipeline(g_Device->device, pipeline->pipeline, g_Allocator);
    pipeline->descriptorSetLayout[MO_PROGRAM_DESC_LAYOUT] = VK_NULL_HANDLE;
    pipeline->descriptorSetLayout[MO_MATERIAL_DESC_LAYOUT] = VK_NULL_HANDLE;
    pipeline->pipelineLayout = VK_NULL_HANDLE;
    pipeline->pipeline = VK_NULL_HANDLE;
    memset(&pipeline->uniformBuffer, 0, sizeof(pipeline->uniformBuffer));
    memset(&pipeline->descriptorSet, 0, sizeof(pipeline->descriptorSet));
    delete pipeline;
}

void moCreateMesh(const MoMeshCreateInfo *pCreateInfo, MoMesh *pMesh)
{
    MoMesh mesh = *pMesh = new MoMesh_T();
    *mesh = {};

    mesh->indexBufferSize = pCreateInfo->indexCount;
    mesh->vertexCount = pCreateInfo->vertexCount;
    const VkDeviceSize index_size = pCreateInfo->indexCount * sizeof(uint32_t);
    createBuffer(g_Device, &mesh->verticesBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    createBuffer(g_Device, &mesh->textureCoordsBuffer, pCreateInfo->vertexCount * sizeof(float2), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    createBuffer(g_Device, &mesh->normalsBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    createBuffer(g_Device, &mesh->tangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    createBuffer(g_Device, &mesh->bitangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    createBuffer(g_Device, &mesh->indexBuffer, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    uploadBuffer(g_Device, mesh->verticesBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pVertices);
    uploadBuffer(g_Device, mesh->textureCoordsBuffer, pCreateInfo->vertexCount * sizeof(float2), pCreateInfo->pTextureCoords);
    uploadBuffer(g_Device, mesh->normalsBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pNormals);
    uploadBuffer(g_Device, mesh->tangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pTangents);
    uploadBuffer(g_Device, mesh->bitangentsBuffer, pCreateInfo->vertexCount * sizeof(float3), pCreateInfo->pBitangents);
    uploadBuffer(g_Device, mesh->indexBuffer, index_size, pCreateInfo->pIndices);
}

void moDestroyMesh(MoMesh mesh)
{
    vkQueueWaitIdle(g_Device->queue);
    deleteBuffer(g_Device, mesh->verticesBuffer);
    deleteBuffer(g_Device, mesh->textureCoordsBuffer);
    deleteBuffer(g_Device, mesh->normalsBuffer);
    deleteBuffer(g_Device, mesh->tangentsBuffer);
    deleteBuffer(g_Device, mesh->bitangentsBuffer);
    deleteBuffer(g_Device, mesh->indexBuffer);
    delete mesh;
}

void moCreateMaterial(const MoMaterialCreateInfo *pCreateInfo, MoMaterial *pMaterial)
{
    MoMaterial material = *pMaterial = new MoMaterial_T();
    *material = {};

    VkResult err = vkDeviceWaitIdle(g_Device->device);
    g_Device->pCheckVkResultFn(err);

    auto & frame = g_SwapChain->frames[g_FrameIndex];
    generateTexture(&material->ambientImage,  pCreateInfo->textureAmbient,  pCreateInfo->colorAmbient,  frame.pool, frame.buffer);
    generateTexture(&material->diffuseImage,  pCreateInfo->textureDiffuse,  pCreateInfo->colorDiffuse,  frame.pool, frame.buffer);
    generateTexture(&material->normalImage,   pCreateInfo->textureNormal,   {0.f, 0.f, 0.f, 0.f},       frame.pool, frame.buffer);
    generateTexture(&material->emissiveImage, pCreateInfo->textureEmissive, pCreateInfo->colorEmissive, frame.pool, frame.buffer);
    generateTexture(&material->specularImage, pCreateInfo->textureSpecular, pCreateInfo->colorSpecular, frame.pool, frame.buffer);

    {
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;
        info.minFilter = info.magFilter = pCreateInfo->textureAmbient.filter;
        err = vkCreateSampler(g_Device->device, &info, g_Allocator, &material->ambientSampler);
        g_Device->pCheckVkResultFn(err);
        info.minFilter = info.magFilter = pCreateInfo->textureDiffuse.filter;
        err = vkCreateSampler(g_Device->device, &info, g_Allocator, &material->diffuseSampler);
        g_Device->pCheckVkResultFn(err);
        info.minFilter = info.magFilter = pCreateInfo->textureNormal.filter;
        err = vkCreateSampler(g_Device->device, &info, g_Allocator, &material->normalSampler);
        g_Device->pCheckVkResultFn(err);
        info.minFilter = info.magFilter = pCreateInfo->textureSpecular.filter;
        err = vkCreateSampler(g_Device->device, &info, g_Allocator, &material->specularSampler);
        g_Device->pCheckVkResultFn(err);
        info.minFilter = info.magFilter = pCreateInfo->textureEmissive.filter;
        err = vkCreateSampler(g_Device->device, &info, g_Allocator, &material->emissiveSampler);
        g_Device->pCheckVkResultFn(err);
    }

    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = g_Device->descriptorPool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &g_Pipeline->descriptorSetLayout[MO_MATERIAL_DESC_LAYOUT];
        err = vkAllocateDescriptorSets(g_Device->device, &alloc_info, &material->descriptorSet);
        g_Device->pCheckVkResultFn(err);
    }

    {
        VkDescriptorImageInfo desc_image[5] = {};
        desc_image[0].sampler = material->ambientSampler;
        desc_image[0].imageView = material->ambientImage->view;
        desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[1].sampler = material->diffuseSampler;
        desc_image[1].imageView = material->diffuseImage->view;
        desc_image[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[2].sampler = material->normalSampler;
        desc_image[2].imageView = material->normalImage->view;
        desc_image[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[3].sampler = material->specularSampler;
        desc_image[3].imageView = material->specularImage->view;
        desc_image[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[4].sampler = material->emissiveSampler;
        desc_image[4].imageView = material->emissiveImage->view;
        desc_image[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write_desc[5] = {};
        for (uint32_t i = 0; i < 5; ++i)
        {
            write_desc[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_desc[i].dstSet = material->descriptorSet;
            write_desc[i].dstBinding = i;
            write_desc[i].descriptorCount = 1;
            write_desc[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_desc[i].pImageInfo = &desc_image[i];
        }
        vkUpdateDescriptorSets(g_Device->device, 5, write_desc, 0, nullptr);
    }
}

void moDestroyMaterial(MoMaterial material)
{
    vkQueueWaitIdle(g_Device->queue);
    deleteBuffer(g_Device, material->ambientImage);
    deleteBuffer(g_Device, material->diffuseImage);
    deleteBuffer(g_Device, material->normalImage);
    deleteBuffer(g_Device, material->specularImage);
    deleteBuffer(g_Device, material->emissiveImage);

    vkDestroySampler(g_Device->device, material->ambientSampler, g_Allocator);
    vkDestroySampler(g_Device->device, material->diffuseSampler, g_Allocator);
    vkDestroySampler(g_Device->device, material->normalSampler, g_Allocator);
    vkDestroySampler(g_Device->device, material->specularSampler, g_Allocator);
    vkDestroySampler(g_Device->device, material->emissiveSampler, g_Allocator);
    delete material;
}

void moBegin(uint32_t frameIndex)
{
    g_FrameIndex = frameIndex;
    auto & frame = g_SwapChain->frames[g_FrameIndex];
    vkCmdBindPipeline(frame.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_Pipeline->pipeline);
    vkCmdBindDescriptorSets(frame.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_Pipeline->pipelineLayout, 0, 1, &g_Pipeline->descriptorSet[g_FrameIndex], 0, nullptr);
}

void moPipelineOverride(MoPipeline pipeline)
{
    if (pipeline == VK_NULL_HANDLE)
    {
        if (g_StashedPipeline != VK_NULL_HANDLE)
            g_Pipeline = g_StashedPipeline;
        g_StashedPipeline = VK_NULL_HANDLE;
    }
    else
    {
        if (g_StashedPipeline == VK_NULL_HANDLE)
            g_StashedPipeline = g_Pipeline;
        g_Pipeline = pipeline;
    }
}

void moSetPMV(const MoPushConstant* pProjectionModelView)
{
    vkCmdPushConstants(g_SwapChain->frames[g_FrameIndex].buffer, g_Pipeline->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MoPushConstant), pProjectionModelView);
}

void moSetLight(const MoUniform* pLightAndCamera)
{
    uploadBuffer(g_Device, g_Pipeline->uniformBuffer[g_FrameIndex], sizeof(MoUniform), pLightAndCamera);
}

void moDrawMesh(MoMesh mesh)
{
    auto & frame = g_SwapChain->frames[g_FrameIndex];

    VkBuffer vertexBuffers[] = {mesh->verticesBuffer->buffer,
                                mesh->textureCoordsBuffer->buffer,
                                mesh->normalsBuffer->buffer,
                                mesh->tangentsBuffer->buffer,
                                mesh->bitangentsBuffer->buffer};
    VkDeviceSize offsets[] = {0,
                              0,
                              0,
                              0,
                              0};
    vkCmdBindVertexBuffers(frame.buffer, 0, 5, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(frame.buffer, mesh->indexBuffer->buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(frame.buffer, mesh->indexBufferSize, 1, 0, 0, 0);
}

void moBindMaterial(MoMaterial material)
{
    auto & frame = g_SwapChain->frames[g_FrameIndex];
    vkCmdBindDescriptorSets(frame.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, g_Pipeline->pipelineLayout, 1, 1, &material->descriptorSet, 0, nullptr);
}

void moFramebufferReadback(VkImage source, VkExtent2D extent, std::uint8_t* pDestination, uint32_t destinationSize, VkCommandPool commandPool)
{
    // Create the linear tiled destination image to copy to and to read the memory from
    VkImageCreateInfo imgCreateInfo = {};
    imgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imgCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imgCreateInfo.extent.width = extent.width;
    imgCreateInfo.extent.height = extent.height;
    imgCreateInfo.extent.depth = 1;
    imgCreateInfo.arrayLayers = 1;
    imgCreateInfo.mipLevels = 1;
    imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
    imgCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    // Create the image
    VkImage dstImage;
    VkResult err = vkCreateImage(g_Device->device, &imgCreateInfo, nullptr, &dstImage);
    g_Device->pCheckVkResultFn(err);
    // Create memory to back up the image
    VkMemoryRequirements memRequirements;
    VkMemoryAllocateInfo memAllocInfo = {};
    memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    VkDeviceMemory dstImageMemory;
    vkGetImageMemoryRequirements(g_Device->device, dstImage, &memRequirements);
    memAllocInfo.allocationSize = memRequirements.size;
    // Memory must be host visible to copy from
    memAllocInfo.memoryTypeIndex = memoryType(g_Device->physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memRequirements.memoryTypeBits);
    err = vkAllocateMemory(g_Device->device, &memAllocInfo, nullptr, &dstImageMemory);
    g_Device->pCheckVkResultFn(err);
    err = vkBindImageMemory(g_Device->device, dstImage, dstImageMemory, 0);
    g_Device->pCheckVkResultFn(err);
    // Do the actual blit from the offscreen image to our host visible destination image
    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = commandPool;
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;
    VkCommandBuffer copyCmd;
    err = vkAllocateCommandBuffers(g_Device->device, &cmdBufAllocateInfo, &copyCmd);
    g_Device->pCheckVkResultFn(err);
    VkCommandBufferBeginInfo cmdBufInfo = {};
    cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    err = vkBeginCommandBuffer(copyCmd, &cmdBufInfo);
    g_Device->pCheckVkResultFn(err);

    auto insertImageMemoryBarrier = [](
        VkCommandBuffer cmdbuffer,
        VkImage image,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask,
        VkImageLayout oldImageLayout,
        VkImageLayout newImageLayout,
        VkPipelineStageFlags srcStageMask,
        VkPipelineStageFlags dstStageMask,
        VkImageSubresourceRange subresourceRange)
    {
        VkImageMemoryBarrier imageMemoryBarrier = {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.srcAccessMask = srcAccessMask;
        imageMemoryBarrier.dstAccessMask = dstAccessMask;
        imageMemoryBarrier.oldLayout = oldImageLayout;
        imageMemoryBarrier.newLayout = newImageLayout;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        vkCmdPipelineBarrier(
            cmdbuffer,
            srcStageMask,
            dstStageMask,
            0,
            0, nullptr,
            0, nullptr,
            1, &imageMemoryBarrier);
    };

    // Transition destination image to transfer destination layout
    insertImageMemoryBarrier(
        copyCmd,
        dstImage,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    // colorAttachment.image is already in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, and does not need to be transitioned

    VkImageCopy imageCopyRegion{};
    imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.srcSubresource.layerCount = 1;
    imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imageCopyRegion.dstSubresource.layerCount = 1;
    imageCopyRegion.extent.width = extent.width;
    imageCopyRegion.extent.height = extent.height;
    imageCopyRegion.extent.depth = 1;

    vkCmdCopyImage(
        copyCmd,
        source, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &imageCopyRegion);

    // Transition destination image to general layout, which is the required layout for mapping the image memory later on
    insertImageMemoryBarrier(
        copyCmd,
        dstImage,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        VK_ACCESS_MEMORY_READ_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    VkResult res = vkEndCommandBuffer(copyCmd);
    g_Device->pCheckVkResultFn(err);

    {
        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &copyCmd;
        VkFenceCreateInfo fenceInfo = {};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence fence;
        res = vkCreateFence(g_Device->device, &fenceInfo, nullptr, &fence);
        g_Device->pCheckVkResultFn(err);
        res = vkQueueSubmit(g_Device->queue, 1, &submitInfo, fence);
        g_Device->pCheckVkResultFn(err);
        res = vkWaitForFences(g_Device->device, 1, &fence, VK_TRUE, UINT64_MAX);
        g_Device->pCheckVkResultFn(err);
        vkDestroyFence(g_Device->device, fence, nullptr);
    }

    // Get layout of the image (including row pitch)
    VkImageSubresource subResource{};
    subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    VkSubresourceLayout subResourceLayout;

    vkGetImageSubresourceLayout(g_Device->device, dstImage, &subResource, &subResourceLayout);

    // Map image memory so we can start copying from it
    const char* imageData = {};
    vkMapMemory(g_Device->device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&imageData);
    imageData += subResourceLayout.offset;

    for (std::uint32_t pixel = 0; pixel < destinationSize && pixel < subResourceLayout.size; pixel += 4)
    {
        pDestination[pixel + 0] = imageData[pixel + 0];
        pDestination[pixel + 1] = imageData[pixel + 1];
        pDestination[pixel + 2] = imageData[pixel + 2];
        pDestination[pixel + 3] = 255; //imageData[pixel + 3];
    }

    // Clean up resources
    vkUnmapMemory(g_Device->device, dstImageMemory);
    vkFreeMemory(g_Device->device, dstImageMemory, nullptr);
    vkDestroyImage(g_Device->device, dstImage, nullptr);
}

void moDefaultMaterial(MoMaterial *pMaterial)
{
    MoMaterialCreateInfo materialInfo = {};
    materialInfo.colorAmbient = { 0.1f, 0.1f, 0.1f, 1.0f };
    materialInfo.colorDiffuse = { 0.64f, 0.64f, 0.64f, 1.0f };
    materialInfo.colorSpecular = { 0.5f, 0.5f, 0.5f, 1.0f };
    materialInfo.colorEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };
    moCreateMaterial(&materialInfo, pMaterial);
}

void moDemoCube(MoMesh *pMesh, const linalg::aliases::float3 & halfExtents)
{
    static float3 cube_positions[] = { { -halfExtents.x, -halfExtents.y, -halfExtents.z },
                                       { -halfExtents.x, -halfExtents.y,  halfExtents.z },
                                       { -halfExtents.x,  halfExtents.y, -halfExtents.z },
                                       { -halfExtents.x,  halfExtents.y,  halfExtents.z },
                                       {  halfExtents.x, -halfExtents.y, -halfExtents.z },
                                       {  halfExtents.x, -halfExtents.y,  halfExtents.z },
                                       {  halfExtents.x,  halfExtents.y, -halfExtents.z },
                                       {  halfExtents.x,  halfExtents.y,  halfExtents.z } };
    static float2 cube_texcoords[] = { { halfExtents.x, 0.0f },
                                       { 0.0f, halfExtents.x },
                                       { 0.0f, 0.0f },
                                       { halfExtents.x, halfExtents.x } };
    static float3 cube_normals[] = { { 0.0f, 1.0f, 0.0f } };
    static mat<unsigned,3,3> cube_triangles[] = { { uint3{ 2, 3, 1 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 4, 7, 3 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 8, 5, 7 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 6, 1, 5 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 7, 1, 3 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 4, 6, 8 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 2, 4, 3 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 4, 8, 7 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 8, 6, 5 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 6, 2, 1 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 7, 5, 1 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                  { uint3{ 4, 2, 6 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } } };

    std::vector<uint32_t> indices;
    uint32_t vertexCount = 0;
    std::vector<float3> vertexPositions;
    std::vector<float2> vertexTexcoords;
    std::vector<float3> vertexNormals;
    std::vector<float3> vertexTangents;
    std::vector<float3> vertexBitangents;
//INDICES_COUNT_FROM_ONE
    for (const auto & triangle : cube_triangles)
    {
        vertexPositions.push_back(cube_positions[triangle.x.x - 1]); vertexTexcoords.push_back(cube_texcoords[triangle.y.x - 1]); vertexNormals.push_back(cube_normals[triangle.z.x - 1]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size());
        vertexPositions.push_back(cube_positions[triangle.x.y - 1]); vertexTexcoords.push_back(cube_texcoords[triangle.y.y - 1]); vertexNormals.push_back(cube_normals[triangle.z.y - 1]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size());
        vertexPositions.push_back(cube_positions[triangle.x.z - 1]); vertexTexcoords.push_back(cube_texcoords[triangle.y.z - 1]); vertexNormals.push_back(cube_normals[triangle.z.z - 1]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size());
    }
    for (uint32_t & index : indices) { --index; }
    for (uint32_t index = 0; index < indices.size(); index+=3)
    {
        vertexCount += 3;

        const uint32_t v1 = indices[index+0];
        const uint32_t v2 = indices[index+1];
        const uint32_t v3 = indices[index+2];

        //discardNormals
        const float3 edge1 = vertexPositions[v2] - vertexPositions[v1];
        const float3 edge2 = vertexPositions[v3] - vertexPositions[v1];
        vertexNormals[v1] = vertexNormals[v2] = vertexNormals[v3] = normalize(cross(edge1, edge2));

        const float2 deltaUV1 = vertexTexcoords[v2] - vertexTexcoords[v1];
        const float2 deltaUV2 = vertexTexcoords[v3] - vertexTexcoords[v1];
        float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (f != 0.f)
        {
            f = 1.0f / f;

            vertexTangents[v1].x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            vertexTangents[v1].y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            vertexTangents[v1].z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            vertexTangents[v1] = vertexTangents[v2] = vertexTangents[v3] = normalize(vertexTangents[v1]);
            vertexBitangents[v1].x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
            vertexBitangents[v1].y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
            vertexBitangents[v1].z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
            vertexBitangents[v1] = vertexBitangents[v2] = vertexBitangents[v3] = normalize(vertexBitangents[v1]);
        }
    }

    MoMeshCreateInfo meshInfo = {};
    meshInfo.indexCount = (uint32_t)indices.size();
    meshInfo.pIndices = indices.data();
    meshInfo.vertexCount = vertexCount;
    meshInfo.pVertices = vertexPositions.data();
    meshInfo.pTextureCoords = vertexTexcoords.data();
    meshInfo.pNormals = vertexNormals.data();
    meshInfo.pTangents = vertexTangents.data();
    meshInfo.pBitangents = vertexBitangents.data();
    moCreateMesh(&meshInfo, pMesh);
}

void moUVSphere(uint32_t meridians, uint32_t parallels, std::vector<float3> & sphere_positions, std::vector<uint32_t> & sphere_indices)
{
    sphere_positions.push_back({0.0f, 1.0f, 0.0f});
    for (uint32_t j = 0; j < parallels - 1; ++j)
    {
        float const polar = 355.0f/113.0f * float(j+1) / float(parallels);
        float const sp = std::sin(polar);
        float const cp = std::cos(polar);
        for (uint32_t i = 0; i < meridians; ++i)
        {
            float const azimuth = 2.0f * 355.0f/113.0f * float(i) / float(meridians);
            float const sa = std::sin(azimuth);
            float const ca = std::cos(azimuth);
            float const x = sp * ca;
            float const y = cp;
            float const z = sp * sa;
            sphere_positions.push_back({x, y, z});
        }
    }
    sphere_positions.push_back({0.0f, -1.0f, 0.0f});

    for (uint32_t i = 0; i < meridians; ++i)
    {
        uint32_t const a = i + 1;
        uint32_t const b = (i + 1) % meridians + 1;
        sphere_indices.emplace_back(0);
        sphere_indices.emplace_back(b);
        sphere_indices.emplace_back(a);
    }

    for (uint32_t j = 0; j < parallels - 2; ++j)
    {
        uint32_t aStart = j * meridians + 1;
        uint32_t bStart = (j + 1) * meridians + 1;
        for (uint32_t i = 0; i < meridians; ++i)
        {
            const uint32_t a = aStart + i;
            const uint32_t a1 = aStart + (i + 1) % meridians;
            const uint32_t b = bStart + i;
            const uint32_t b1 = bStart + (i + 1) % meridians;
            sphere_indices.emplace_back(a);
            sphere_indices.emplace_back(a1);
            sphere_indices.emplace_back(b1);

            sphere_indices.emplace_back(a);
            sphere_indices.emplace_back(b1);
            sphere_indices.emplace_back(b);
        }
    }

    for (uint32_t i = 0; i < meridians; ++i)
    {
        uint32_t const a = i + meridians * (parallels - 2) + 1;
        uint32_t const b = (i + 1) % meridians + meridians * (parallels - 2) + 1;
        sphere_indices.emplace_back(sphere_positions.size() - 1);
        sphere_indices.emplace_back(a);
        sphere_indices.emplace_back(b);
    }
}

void moDemoSphere(MoMesh *pMesh)
{
    static float2 sphere_texcoords[] = {{ 0.0f, 0.0f }};
    std::vector<float3> sphere_positions;
    std::vector<uint32_t> sphere_indices;
    moUVSphere(64, 32, sphere_positions, sphere_indices);
    const std::vector<float3> & sphere_normals = sphere_positions;

    std::vector<mat<unsigned,3,3>> sphere_triangles;
    for (uint32_t i = 0; i < sphere_indices.size(); i+=3)
    {
        sphere_triangles.push_back({uint3{sphere_indices[i+0], sphere_indices[i+1], sphere_indices[i+2]},
                                    uint3{0,0,0},
                                    uint3{sphere_indices[i+0], sphere_indices[i+1], sphere_indices[i+2]}});
    }

    std::vector<uint32_t> indices;
    uint32_t vertexCount = 0;
    std::vector<float3> vertexPositions;
    std::vector<float2> vertexTexcoords;
    std::vector<float3> vertexNormals;
    std::vector<float3> vertexTangents;
    std::vector<float3> vertexBitangents;
//INDICES_COUNT_FROM_ONE
    for (const auto & triangle : sphere_triangles)
    {
        vertexPositions.push_back(sphere_positions[triangle.x.x]);vertexTexcoords.push_back(sphere_texcoords[triangle.y.x]); vertexNormals.push_back(sphere_normals[triangle.z.x]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size() - 1);
        vertexPositions.push_back(sphere_positions[triangle.x.y]); vertexTexcoords.push_back(sphere_texcoords[triangle.y.y]); vertexNormals.push_back(sphere_normals[triangle.z.y]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size() - 1);
        vertexPositions.push_back(sphere_positions[triangle.x.z]); vertexTexcoords.push_back(sphere_texcoords[triangle.y.z]); vertexNormals.push_back(sphere_normals[triangle.z.z]); vertexTangents.push_back({1.0f, 0.0f, 0.0f}); vertexBitangents.push_back({0.0f, 0.0f, 1.0f}); indices.push_back((uint32_t)vertexPositions.size() - 1);
    }
    for (uint32_t index = 0; index < indices.size(); index+=3)
    {
        vertexCount += 3;

        const uint32_t v1 = indices[index+0];
        const uint32_t v2 = indices[index+1];
        const uint32_t v3 = indices[index+2];

        const float3 edge1 = vertexPositions[v2] - vertexPositions[v1];
        const float3 edge2 = vertexPositions[v3] - vertexPositions[v1];
        const float2 deltaUV1 = vertexTexcoords[v2] - vertexTexcoords[v1];
        const float2 deltaUV2 = vertexTexcoords[v3] - vertexTexcoords[v1];
        float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (f != 0.f)
        {
            f = 1.0f / f;

            vertexTangents[v1].x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            vertexTangents[v1].y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            vertexTangents[v1].z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            vertexTangents[v1] = vertexTangents[v2] = vertexTangents[v3] = normalize(vertexTangents[v1]);
            vertexBitangents[v1].x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
            vertexBitangents[v1].y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
            vertexBitangents[v1].z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
            vertexBitangents[v1] = vertexBitangents[v2] = vertexBitangents[v3] = normalize(vertexBitangents[v1]);
        }
    }

    MoMeshCreateInfo meshInfo = {};
    meshInfo.indexCount = (uint32_t)indices.size();
    meshInfo.pIndices = indices.data();
    meshInfo.vertexCount = vertexCount;
    meshInfo.pVertices = vertexPositions.data();
    meshInfo.pTextureCoords = vertexTexcoords.data();
    meshInfo.pNormals = vertexNormals.data();
    meshInfo.pTangents = vertexTangents.data();
    meshInfo.pBitangents = vertexBitangents.data();
    moCreateMesh(&meshInfo, pMesh);
}

void moDemoMaterial(MoMaterial *pMaterial)
{
    const uint32_t diffuse[8*8] = {0xff1a07e3,0xff48f4fb,0xff66b21d,0xfff9fb00,0xffa91f6c,0xffb98ef1,0xffb07279,0xff6091f7,
                                   0xff6091f7,0xff1a07e3,0xff48f4fb,0xff66b21d,0xfff9fb00,0xffa91f6c,0xffb98ef1,0xffb07279,
                                   0xffb07279,0xff6091f7,0xff1a07e3,0xff48f4fb,0xff66b21d,0xfff9fb00,0xffa91f6c,0xffb98ef1,
                                   0xffb98ef1,0xffb07279,0xff6091f7,0xff1a07e3,0xff48f4fb,0xff66b21d,0xfff9fb00,0xffa91f6c,
                                   0xffa91f6c,0xffb98ef1,0xffb07279,0xff6091f7,0xff1a07e3,0xff48f4fb,0xff66b21d,0xfff9fb00,
                                   0xfff9fb00,0xffa91f6c,0xffb98ef1,0xffb07279,0xff6091f7,0xff1a07e3,0xff48f4fb,0xff66b21d,
                                   0xff66b21d,0xfff9fb00,0xffa91f6c,0xffb98ef1,0xffb07279,0xff6091f7,0xff1a07e3,0xff48f4fb,
                                   0xff48f4fb,0xff66b21d,0xfff9fb00,0xffa91f6c,0xffb98ef1,0xffb07279,0xff6091f7,0xff1a07e3};

    MoMaterialCreateInfo materialInfo = {};
    materialInfo.colorAmbient = { 0.2f, 0.2f, 0.2f, 1.0f };
    materialInfo.colorDiffuse = { 0.64f, 0.64f, 0.64f, 1.0f };
    materialInfo.colorSpecular = { 0.5f, 0.5f, 0.5f, 1.0f };
    materialInfo.colorEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };
    materialInfo.textureDiffuse.pData = (uint8_t*)diffuse;
    materialInfo.textureDiffuse.extent = { 8, 8 };
    moCreateMaterial(&materialInfo, pMaterial);
}

void moGridMaterial(MoMaterial *pMaterial)
{
    const uint32_t diffuse[8*8] = {0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,0xffffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,
                                   0xffffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff,0x00ffffff};

    MoMaterialCreateInfo materialInfo = {};
    materialInfo.colorAmbient = { 0.2f, 0.2f, 0.2f, 1.0f };
    materialInfo.colorDiffuse = { 0.64f, 0.64f, 0.64f, 1.0f };
    materialInfo.colorSpecular = { 0.5f, 0.5f, 0.5f, 1.0f };
    materialInfo.colorEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };
    materialInfo.textureDiffuse.pData = (uint8_t*)diffuse;
    materialInfo.textureDiffuse.extent = { 8, 8 };
    moCreateMaterial(&materialInfo, pMaterial);
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
