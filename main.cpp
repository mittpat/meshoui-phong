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

static MoFloat3 cube_positions[] = { { -1.0f, -1.0f, -1.0f },
                                     { -1.0f, -1.0f,  1.0f },
                                     { -1.0f,  1.0f, -1.0f },
                                     { -1.0f,  1.0f,  1.0f },
                                     { 1.0f, -1.0f, -1.0f },
                                     { 1.0f, -1.0f,  1.0f },
                                     { 1.0f,  1.0f, -1.0f },
                                     { 1.0f,  1.0f,  1.0f } };
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

        proj_matrix = mul(corr_matrix, perspective_matrix(degreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

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
#define INDICESCOUNTFROMONE
#ifdef INDICESCOUNTFROMONE
        for (const auto & triangle : cube_triangles)
        {
            vertices.emplace_back(MoVertex{ cube_positions[triangle.x.x - 1], cube_texcoords[triangle.y.x - 1], cube_normals[triangle.z.x - 1], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size());
            vertices.emplace_back(MoVertex{ cube_positions[triangle.x.y - 1], cube_texcoords[triangle.y.y - 1], cube_normals[triangle.z.y - 1], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size());
            vertices.emplace_back(MoVertex{ cube_positions[triangle.x.z - 1], cube_texcoords[triangle.y.z - 1], cube_normals[triangle.z.z - 1], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size());
        }
        for (uint32_t & index : indices) { --index; }
#else
        for (const auto & triangle : cube_triangles)
        {
            vertices.emplace_back(MoVertex{ cube_positions[triangle.x.x], cube_texcoords[triangle.y.x], cube_normals[triangle.z.x], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size()-1);
            vertices.emplace_back(MoVertex{ cube_positions[triangle.x.y], cube_texcoords[triangle.y.y], cube_normals[triangle.z.y], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size()-1);
            vertices.emplace_back(MoVertex{ cube_positions[triangle.x.z], cube_texcoords[triangle.y.z], cube_normals[triangle.z.z], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size()-1);
        }
#endif
        for (uint32_t index = 0; index < indices.size(); index+=3)
        {
            MoVertex &v1 = vertices[indices[index+0]];
            MoVertex &v2 = vertices[indices[index+1]];
            MoVertex &v3 = vertices[indices[index+2]];

            //discardNormals
            const float3 edge1 = (float3&)v2.position - (float3&)v1.position;
            const float3 edge2 = (float3&)v3.position - (float3&)v1.position;
            (float3&)v1.normal = (float3&)v2.normal = (float3&)v3.normal = normalize(cross(edge1, edge2));

#define GENERATETANGENTS
#ifdef GENERATETANGENTS
            const float2 deltaUV1 = (float2&)v2.texcoord - (float2&)v1.texcoord;
            const float2 deltaUV2 = (float2&)v3.texcoord - (float2&)v1.texcoord;
            float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
            if (f != 0.f)
            {
                f = 1.0f / f;

                v1.tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
                v1.tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
                v1.tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
                (float3&)v1.tangent = (float3&)v2.tangent = (float3&)v3.tangent = normalize((float3&)v1.tangent);
                v1.bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
                v1.bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
                v1.bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
                (float3&)v1.bitangent = (float3&)v2.bitangent = (float3&)v3.bitangent = normalize((float3&)v1.bitangent);
            }
#endif
        }

        MoMeshCreateInfo meshInfo = {};
        meshInfo.indexCount = indicesCount = (uint32_t)indices.size();
        meshInfo.pIndices = indices.data();
        meshInfo.vertexCount = (uint32_t)vertices.size();
        meshInfo.pVertices = vertices.data();
        moCreateMesh(&meshInfo, &cube);

        MoMaterialCreateInfo materialInfo = {};
        materialInfo.colorAmbient = { 0.1f, 0.1f, 0.1f, 1.0f };
        materialInfo.colorDiffuse = { 0.64f, 0.64f, 0.64f, 1.0f };
        materialInfo.colorSpecular = { 0.5f, 0.5f, 0.5f, 1.0f };
        materialInfo.colorEmissive = { 0.0f, 0.0f, 0.0f, 1.0f };
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
        {
            model_matrix = mul(model_matrix, linalg::rotation_matrix(linalg::rotation_quat({0.0f,1.0f,0.0f}, 0.01f)));

            static MoPushConstant pmv = {};
            (float4x4&)pmv.projection = proj_matrix;
            (float4x4&)pmv.model = model_matrix;
            (float4x4&)pmv.view = view_matrix;
            moSetPMV(&pmv);

            static MoUniform uni = {};
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
    {
        moDestroyMaterial(material);
        moDestroyMesh(cube);
    }

    // Cleanup
    {
        moShutdown();
        moDestroySwapChain(device, swapChain);
        vkDestroySurfaceKHR(instance, surface, allocator);
        moDestroyDevice(device);
        moDestroyInstance(instance);

        glfwDestroyWindow(window);
        glfwTerminate();
    }

    return 0;
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
