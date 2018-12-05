#pragma once

#include <vulkan/vulkan.h>

namespace Meshoui
{
    struct DeviceVk;
    struct InstanceVk final
    {
        ~InstanceVk();
        InstanceVk();

        void create(const char* const* extensions, uint32_t extensionsCount);
        void destroy();

        VkInstance instance;
        VkAllocationCallbacks* allocator;
    };
    inline InstanceVk::~InstanceVk() {}
    inline InstanceVk::InstanceVk() : instance(VK_NULL_HANDLE), allocator(VK_NULL_HANDLE) {}
}
