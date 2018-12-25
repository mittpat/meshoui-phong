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

int main(int, char**)
{
    GLFWwindow*                  window;
    VkInstance                   instance = VK_NULL_HANDLE;
    MoDevice                     device;
    MoSwapChain                  swapChain;
    VkSurfaceKHR                 surface;
    VkSurfaceFormatKHR           surfaceFormat;
    uint32_t                     frameIndex = 0;
    const VkAllocationCallbacks* allocator = VK_NULL_HANDLE;

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
    uint32_t indicesCount = 0;
    MoMesh cube;
    MoMaterial material;
    {
        MoInitInfo2 initInfo = {};
        initInfo.instance = instance;
        initInfo.device = device;
        initInfo.swapChain = swapChain;
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
        VkSemaphore imageAcquiredSemaphore;
        moBeginSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);
        moNewFrame(frameIndex);
        moBindMaterial(material);
        moSetPMV((MoFloat4x4&)linalg_proj_matrix, (MoFloat4x4&)linalg_model_matrix, (MoFloat4x4&)linalg_view_matrix);
        moSetLight((MoFloat3&)linalg_light_position, { linalg_camera_matrix.w.x, linalg_camera_matrix.w.y, linalg_camera_matrix.w.z });
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
    {
        moDestroyMaterial(material);
        moDestroyMesh(cube);
    }

    // Cleanup
    {
        moDestroySwapChain(swapChain);
        vkDestroySurfaceKHR(instance, surface, allocator);
        moDestroyDevice(device);
        moDestroyInstance(instance);
        moShutdown();

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    return 0;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
