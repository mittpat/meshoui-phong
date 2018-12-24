#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "DeviceVk.h"
#include "SwapChainVk.h"
#include "phong.h"

#include <linalg.h>

#include <cstdio>
#include <cstdlib>

static void glfw_error_callback(int, const char* description)
{
    printf("Error: %s\n", description);
}

static void vk_check_result(VkResult err)
{
    if (err == 0) return;
    printf("VkResult %d\n", err);
    if (err < 0)
        abort();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
    (void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
    fprintf(stderr, "[vulkan] ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
    return VK_FALSE;
}

static constexpr float degreesToRadians(float angle)
{
    return angle * 3.14159265359f / 180.0f;
}

using namespace linalg;
using namespace linalg::aliases;

static MoFloat3 cube_positions[] = { { -2.0f, -2.0f, -2.0f },
                                     { -2.0f, -2.0f,  2.0f },
                                     { -2.0f,  2.0f, -2.0f },
                                     { -2.0f,  2.0f,  2.0f },
                                     { 2.0f, -2.0f, -2.0f },
                                     { 2.0f, -2.0f,  2.0f },
                                     { 2.0f,  2.0f, -2.0f },
                                     { 2.0f,  2.0f,  2.0f } };
static MoFloat2 cube_texcoords[] = { { 1, 0 },
                                     { 0, 1 },
                                     { 0, 0 },
                                     { 1, 1 } };
static MoFloat3 cube_normals[] = { { 0.0f, 1.0f, 0.0f } };
static MoUInt3x3 cube_triangles[] = { { { 2, 3, 1 },{ 1, 2, 3 },{ 1,1,1 } },
                                      { { 4, 7, 3 },{ 1, 2, 3 },{ 1,1,1 } },
                                      { { 8, 5, 7 },{ 1, 2, 3 },{ 1,1,1 } },
                                      { { 6, 1, 5 },{ 1, 2, 3 },{ 1,1,1 } },
                                      { { 7, 1, 3 },{ 1, 2, 3 },{ 1,1,1 } },
                                      { { 4, 6, 8 },{ 1, 2, 3 },{ 1,1,1 } },
                                      { { 2, 4, 3 },{ 1, 4, 2 },{ 1,1,1 } },
                                      { { 4, 8, 7 },{ 1, 4, 2 },{ 1,1,1 } },
                                      { { 8, 6, 5 },{ 1, 4, 2 },{ 1,1,1 } },
                                      { { 6, 2, 1 },{ 1, 4, 2 },{ 1,1,1 } },
                                      { { 7, 5, 1 },{ 1, 4, 2 },{ 1,1,1 } },
                                      { { 4, 2, 6 },{ 1, 4, 2 },{ 1,1,1 } } };
static float4x4 corr_matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
                                { 0.0f,-1.0f, 0.0f, 0.0f },
                                { 0.0f, 0.0f, 0.5f, 0.0f },
                                { 0.0f, 0.0f, 0.5f, 1.0f } };
static float4x4 linalg_proj_matrix = mul(corr_matrix, perspective_matrix(degreesToRadians(100.f), 1920 / 1080.f, 0.1f, 1000.f));
static float4x4 linalg_camera_matrix = translation_matrix(float3{ 3.0f, 0.0f, 5.0f });
static float4x4 linalg_view_matrix = inverse(linalg_camera_matrix);
static float4x4 linalg_model_matrix = identity;
static float3   linalg_light_position = { 500.0f, 1000.0f, 500.0f };

using namespace Meshoui;

int main(int, char**)
{
    GLFWwindow*        window;
    VkInstance         instance;
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
        window = glfwCreateWindow(1920 / 2, 1080 / 2, "Graphics Previewer", nullptr, nullptr);
        if (!glfwVulkanSupported()) printf("GLFW: Vulkan Not Supported\n");

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
        device.create(instance);

        // Create Window Surface
        VkResult err = glfwCreateWindowSurface(instance, window, device.allocator, &surface);
        vk_check_result(err);

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

    // Meshoui initialization
    uint32_t indicesCount = 0;
    MoMesh cube;
    MoMaterial material;
    {
        MoInitInfo initInfo = {};
        initInfo.instance = instance;
        initInfo.physicalDevice = device.physicalDevice;
        initInfo.device = device.device;
        initInfo.queueFamily = device.queueFamily;
        initInfo.queue = device.queue;
        initInfo.descriptorPool = device.descriptorPool;
        initInfo.swapChainKHR = swapChain.swapChainKHR;
        initInfo.renderPass = swapChain.renderPass;
        MoImageBufferInfo swapChainImageBufferInfo[FrameCount];
        for (uint32_t i = 0; i < FrameCount; ++i)
        {
            swapChainImageBufferInfo[i].back = swapChain.images[i].back;
            swapChainImageBufferInfo[i].view = swapChain.images[i].view;
            swapChainImageBufferInfo[i].front = swapChain.images[i].front;
        }
        initInfo.pSwapChainImageBuffers = swapChainImageBufferInfo;
        initInfo.swapChainImageBufferCount = FrameCount;
        MoCommandBufferInfo swapChainCommandBufferInfo[FrameCount];
        for (uint32_t i = 0; i < FrameCount; ++i)
        {
            swapChainCommandBufferInfo[i].pool = swapChain.frames[i].pool;
            swapChainCommandBufferInfo[i].buffer = swapChain.frames[i].buffer;
            swapChainCommandBufferInfo[i].fence = swapChain.frames[i].fence;
            swapChainCommandBufferInfo[i].acquired = swapChain.frames[i].acquired;
            swapChainCommandBufferInfo[i].complete = swapChain.frames[i].complete;
        }
        initInfo.pSwapChainCommandBuffers = swapChainCommandBufferInfo;
        initInfo.swapChainCommandBufferCount = FrameCount;
        initInfo.extent = swapChain.extent;
        initInfo.pAllocator = device.allocator;
        initInfo.pCheckVkResultFn = vk_check_result;
        moInit(&initInfo);

        std::vector<uint32_t> indices;
        std::vector<MoVertex> vertices;
        for (const auto & triangle : cube_triangles)
        {
            vertices.emplace_back(MoVertex{ cube_positions[triangle.x.x - 1], cube_texcoords[triangle.y.x - 1], cube_normals[triangle.z.x - 1] }); indices.push_back((uint32_t)vertices.size());
            vertices.emplace_back(MoVertex{ cube_positions[triangle.x.y - 1], cube_texcoords[triangle.y.y - 1], cube_normals[triangle.z.y - 1] }); indices.push_back((uint32_t)vertices.size());
            vertices.emplace_back(MoVertex{ cube_positions[triangle.x.z - 1], cube_texcoords[triangle.y.z - 1], cube_normals[triangle.z.z - 1] }); indices.push_back((uint32_t)vertices.size());
        }

        MoMeshCreateInfo meshInfo = {};
        meshInfo.indexCount = indicesCount = (uint32_t)indices.size();
        meshInfo.pIndices = indices.data();
        meshInfo.vertexCount = (uint32_t)vertices.size();
        meshInfo.pVertices = vertices.data();
        meshInfo.discardNormals = VK_TRUE;
        meshInfo.indicesCountFromOne = VK_TRUE;
        moCreateMesh(&meshInfo, &cube);

        MoMaterialCreateInfo materialInfo = {};
        materialInfo.colorAmbient = { 0.1f, 0.1f, 0.1f };
        materialInfo.colorDiffuse = { 0.64f, 0.64f, 0.64f };
        materialInfo.colorSpecular = { 0.5f, 0.5f, 0.5f };
        materialInfo.colorEmissive = { 0.0f, 0.0f, 0.0f };
        moCreateMaterial(&materialInfo, &material);
    }

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Frame begin
        VkSemaphore& imageAcquiredSemaphore = swapChain.beginRender(device, frameIndex);

        moNewFrame(frameIndex);
        moBindMaterial(material);
        moBindMesh(cube);
        moSetPMV((MoFloat4x4&)linalg_proj_matrix, (MoFloat4x4&)linalg_model_matrix, (MoFloat4x4&)linalg_view_matrix);
        moSetLight((MoFloat3&)linalg_light_position, { linalg_camera_matrix.w.x, linalg_camera_matrix.w.y, linalg_camera_matrix.w.z });
        auto & frame = swapChain.frames[frameIndex];
        VkViewport viewport{ 0, 0, float(swapChain.extent.width), float(swapChain.extent.height), 0.f, 1.f };
        vkCmdSetViewport(frame.buffer, 0, 1, &viewport);
        VkRect2D scissor{ { 0, 0 },{ swapChain.extent.width, swapChain.extent.height } };
        vkCmdSetScissor(frame.buffer, 0, 1, &scissor);
        vkCmdDrawIndexed(frame.buffer, indicesCount, 1, 0, 0, 0);

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
            swapChain.createImageBuffers(device, depthBuffer, surface, surfaceFormat, width, height, true);
            err = VK_SUCCESS;
        }
        vk_check_result(err);
    }

    // Meshoui cleanup
    {
        moDestroyMaterial(material);
        moDestroyMesh(cube);
        moShutdown();
    }

    // Cleanup
    {
        VkResult err = vkDeviceWaitIdle(device.device);
        vk_check_result(err);

        swapChain.destroyImageBuffers(device, depthBuffer);
        swapChain.destroyCommandBuffers(device);
        vkDestroySurfaceKHR(instance, surface, device.allocator);
        device.destroy();
        moDestroyInstance(instance);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    return 0;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
