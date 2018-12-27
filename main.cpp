#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "phong.h"

#include <linalg.h>

#include <cstdio>
#include <cstdlib>
#include <vector>

static void glfw_error_callback(int, const char* description)
{
    printf("GLFW Error: %s\n", description);
}

static void vk_check_result(VkResult err)
{
    if (err == 0) return;
    printf("[vulkan] Result: %d\n", err);
    if (err < 0)
        abort();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_report(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT objectType, uint64_t, size_t, int32_t, const char*, const char* pMessage, void*)
{
    printf("[vulkan] ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
    return VK_FALSE;
}

static constexpr float degreesToRadians(float angle)
{
    return angle * 3.14159265359f / 180.0f;
}

using namespace linalg;
using namespace linalg::aliases;

static float4x4 corr_matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
                                { 0.0f,-1.0f, 0.0f, 0.0f },
                                { 0.0f, 0.0f, 1.0f, 0.0f },
                                { 0.0f, 0.0f, 0.0f, 1.0f } };
static float4x4 proj_matrix = mul(corr_matrix, perspective_matrix(degreesToRadians(75.f), 1920 / 1080.f, 0.1f, 1000.f, pos_z, zero_to_one));
static float4x4 camera_matrix = translation_matrix(float3{ 3.0f, 0.0f, 5.0f });
static float4x4 view_matrix = inverse(camera_matrix);
static float4x4 model_matrix = identity;
static float3   light_position = { 500.0f, 1000.0f, 500.0f };

int main(int, char**)
{
    GLFWwindow*                  window = nullptr;
    VkInstance                   instance = VK_NULL_HANDLE;
    MoDevice                     device = VK_NULL_HANDLE;
    MoSwapChain                  swapChain = VK_NULL_HANDLE;
    VkSurfaceKHR                 surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR           surfaceFormat = {};
    uint32_t                     frameIndex = 0;
    VkPipelineCache              pipelineCache = VK_NULL_HANDLE;
    const VkAllocationCallbacks* allocator = VK_NULL_HANDLE;

    // Initialization
    {
        glfwSetErrorCallback(glfw_error_callback);
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(1920 / 2, 1080 / 2, "Graphics Previewer", nullptr, nullptr);
        if (!glfwVulkanSupported()) printf("GLFW: Vulkan Not Supported\n");

        // Create Vulkan instance
        {
            MoInstanceCreateInfo createInfo = {};
            createInfo.pExtensions = glfwGetRequiredInstanceExtensions(&createInfo.extensionsCount);
#ifdef _DEBUG
            createInfo.debugReport = VK_TRUE;
            createInfo.pDebugReportCallback = vk_debug_report;
#endif
            createInfo.pCheckVkResultFn = vk_check_result;
            moCreateInstance(&createInfo, &instance);
        }

        // Create Window Surface
        VkResult err = glfwCreateWindowSurface(instance, window, allocator, &surface);
        vk_check_result(err);

        // Create Framebuffers
        int width = 0, height = 0;
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwPostEmptyEvent();
            glfwWaitEvents();
        }

        proj_matrix = mul(corr_matrix, perspective_matrix(degreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

        // Create device
        {
            VkFormat requestFormats[4] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
            MoDeviceCreateInfo createInfo = {};
            createInfo.instance = instance;
            createInfo.surface = surface;
            createInfo.pRequestFormats = requestFormats;
            createInfo.requestFormatsCount = 4;
            createInfo.requestColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
            createInfo.pSurfaceFormat = &surfaceFormat;
            createInfo.pCheckVkResultFn = vk_check_result;
            moCreateDevice(&createInfo, &device);
        }

        // Create SwapChain, RenderPass, Framebuffer, etc.
        {
            MoSwapChainCreateInfo createInfo = {};
            createInfo.device = device;
            createInfo.surface = surface;
            createInfo.surfaceFormat = surfaceFormat;
            createInfo.extent = {(uint32_t)width, (uint32_t)height};
            createInfo.vsync = VK_TRUE;
            createInfo.pAllocator = allocator;
            createInfo.pCheckVkResultFn = vk_check_result;
            moCreateSwapChain(&createInfo, &swapChain);
        }
    }

    // Meshoui initialization
    {
        MoInitInfo initInfo = {};
        initInfo.instance = instance;
        initInfo.physicalDevice = device->physicalDevice;
        initInfo.device = device->device;
        initInfo.queueFamily = device->queueFamily;
        initInfo.queue = device->queue;
        initInfo.pipelineCache = pipelineCache;
        initInfo.descriptorPool = device->descriptorPool;
        initInfo.pSwapChainSwapBuffers = swapChain->images;
        initInfo.swapChainSwapBufferCount = MO_FRAME_COUNT;
        initInfo.pSwapChainCommandBuffers = swapChain->frames;
        initInfo.swapChainCommandBufferCount = MO_FRAME_COUNT;
        initInfo.depthBuffer = swapChain->depthBuffer;
        initInfo.swapChainKHR = swapChain->swapChainKHR;
        initInfo.renderPass = swapChain->renderPass;
        initInfo.extent = swapChain->extent;
        initInfo.pAllocator = allocator;
        initInfo.pCheckVkResultFn = device->pCheckVkResultFn;
        moInit(&initInfo);
    }

    // Demo
    MoMesh cube;
    moDemoCube(&cube);
    MoMaterial material;
    moDemoMaterial(&material);

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Frame begin
        VkSemaphore imageAcquiredSemaphore;
        moBeginSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);
        moNewFrame(frameIndex);
        moBindMaterial(material);
        {
            model_matrix = mul(model_matrix, linalg::rotation_matrix(linalg::rotation_quat({0.0f, 1.0f, 0.0f}, 0.01f)));

            MoPushConstant pmv = {};
            (float4x4&)pmv.projection = proj_matrix;
            (float4x4&)pmv.model = model_matrix;
            (float4x4&)pmv.view = view_matrix;
            moSetPMV(&pmv);

            MoUniform uni = {};
            (float3&)uni.light = light_position;
            (float3&)uni.camera = camera_matrix.w.xyz();
            moSetLight(&uni);
        }
        moDrawMesh(cube);

        // Frame end
        VkResult err = moEndSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            int width = 0, height = 0;
            while (width == 0 || height == 0)
            {
                glfwGetFramebufferSize(window, &width, &height);
                glfwPostEmptyEvent();
                glfwWaitEvents();
            }

            proj_matrix = mul(corr_matrix, perspective_matrix(degreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

            MoSwapChainRecreateInfo recreateInfo = {};
            recreateInfo.surface = surface;
            recreateInfo.surfaceFormat = surfaceFormat;
            recreateInfo.extent = {(uint32_t)width, (uint32_t)height};
            recreateInfo.vsync = VK_TRUE;
            moRecreateSwapChain(&recreateInfo, swapChain);
            err = VK_SUCCESS;
        }
        vk_check_result(err);
    }

    // Meshoui cleanup
    moDestroyMaterial(material);
    moDestroyMesh(cube);

    // Cleanup
    moShutdown();
    moDestroySwapChain(device, swapChain);
    vkDestroySurfaceKHR(instance, surface, allocator);
    moDestroyDevice(device);
    moDestroyInstance(instance);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
