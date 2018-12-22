#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vk/DeviceVk.h>
#include <vk/InstanceVk.h>
#include <vk/SwapChainVk.h>

#include <Meshoui.h>

#include <cstdio>
#include <cstdlib>

static void glfw_error_callback(int, const char* description)
{
    printf("Error: %s\n", description);
}

static void check_vk_result(VkResult err)
{
    if (err == 0) return;
    printf("VkResult %d\n", err);
    if (err < 0)
        abort();
}

using namespace Meshoui;

int main(int, char**)
{
    GLFWwindow*        window;
    InstanceVk         instance;
    DeviceVk           device;
    SwapChainVk        swapChain;
    ImageBufferVk      depthBuffer;
    VkSurfaceKHR       surface;
    VkSurfaceFormatKHR surfaceFormat;
    uint32_t           frameIndex = 0;

    // Initialization
    {
        glfwSetErrorCallback(glfw_error_callback);
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(1920/2, 1080/2, "Meshoui", nullptr, nullptr);
        if (!glfwVulkanSupported()) printf("GLFW: Vulkan Not Supported\n");

        uint32_t extensionsCount = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions(&extensionsCount);
        instance.create(extensions, extensionsCount);
        device.create(instance);

        // Create Window Surface
        VkResult err = glfwCreateWindowSurface(instance.instance, window, device.allocator, &surface);
        check_vk_result(err);

        // Create Framebuffers
        int width = 0, height = 0;
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwPostEmptyEvent();
            glfwWaitEvents();
        }

        // Check for WSI support
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(device.physicalDevice, device.queueFamily, surface, &res);
        if (res != VK_TRUE)
        {
            fprintf(stderr, "Error no WSI support on physical device 0\n");
            abort();
        }
        device.selectSurfaceFormat(surface, surfaceFormat, { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM }, VK_COLORSPACE_SRGB_NONLINEAR_KHR);

        // Create SwapChain, RenderPass, Framebuffer, etc.
        swapChain.createCommandBuffers(device);
        swapChain.createImageBuffers(device, depthBuffer, surface, surfaceFormat, width, height, true);
    }

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Frame begin
        VkSemaphore& imageAcquiredSemaphore  = swapChain.beginRender(device, frameIndex);




        // Frame end
        VkResult err = swapChain.endRender(imageAcquiredSemaphore, device.queue, frameIndex);
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            int width = 0, height = 0;
            while (width == 0 || height == 0)
            {
                glfwGetFramebufferSize(window, &width, &height);
                glfwPostEmptyEvent();
                glfwWaitEvents();
            }

            vkDeviceWaitIdle(device.device);
            swapChain.createImageBuffers(device, depthBuffer, surface, surfaceFormat, width, height, false);
            err = VK_SUCCESS;
        }
        check_vk_result(err);
    }

    // Cleanup
    {
        VkResult err = vkDeviceWaitIdle(device.device);
        check_vk_result(err);

        swapChain.destroyImageBuffers(device, depthBuffer);
        swapChain.destroyCommandBuffers(device);
        vkDestroySurfaceKHR(instance.instance, surface, instance.allocator);
        device.destroy();
        instance.destroy();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    return 0;
}
