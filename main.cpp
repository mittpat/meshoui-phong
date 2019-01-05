#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "collada.h"
#include "phong.h"
#include "vertexformat.h"

#include <linalg.h>

#include <cstdio>
#include <cstdlib>
#include <experimental/filesystem>
#include <functional>
#include <fstream>
#include <streambuf>
#include <vector>

namespace std { namespace filesystem = experimental::filesystem; }

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
static float4x4 camera_matrix = translation_matrix(float3{ 3.0f, 0.0f, 15.0f });
static float4x4 view_matrix = inverse(camera_matrix);
static float4x4 model_matrix = identity;
static float3   light_position = { 500.0f, 1000.0f, 500.0f };

int main(int argc, char** argv)
{
#ifdef _DEBUG
    moTestVertexFormat();
#endif
    const char * filename = nullptr;
    if (argc > 1)
    {
        filename = argv[1];
        if (std::filesystem::path(filename).extension() != ".dae" || !std::filesystem::exists(filename))
        {
            printf("Usage: meshouiview [options] file          \n"
                   "Options:                                   \n"
                   "  --help Display this information.         \n"
                   "                                           \n"
                   "For bug reporting instructions, please see:\n"
                   "<https://github.com/mittpat/meshoui>.      \n");
            return 0;
        }
    }

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

    struct Drawable {
        float4x4 model;
        MoMesh   mesh;
    };
    std::vector<MoMesh> meshes;
    std::vector<Drawable> drawables;

    MoMaterial material = {};
    if (filename != nullptr)
    {
        MoColladaData collada;

        {
            std::ifstream fileStream(filename);
            std::string contents((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
            MoColladaDataCreateInfo createInfo;
            createInfo.pContents = contents.data();
            moCreateColladaData(&createInfo, &collada);
        }

        meshes.resize(collada->meshCount, 0);
        for (uint32_t i = 0; i < collada->meshCount; ++i)
        {
            MoColladaMesh colladaMesh = collada->pMeshes[i];

            MoVertexFormat vertexFormat;

            std::vector<MoVertexAttribute> attributes(3);
            attributes[0].pAttribute = &colladaMesh->pVertices->data[0];
            attributes[0].attributeCount = colladaMesh->vertexCount;
            attributes[0].componentCount = 3;
            attributes[1].pAttribute = &colladaMesh->pTexcoords->data[0];
            attributes[1].attributeCount = colladaMesh->texcoordCount;
            attributes[1].componentCount = 2;
            attributes[2].pAttribute = &colladaMesh->pNormals->data[0];
            attributes[2].attributeCount = colladaMesh->normalCount;
            attributes[2].componentCount = 3;

            MoVertexFormatCreateInfo createInfo = {};
            createInfo.pAttributes = attributes.data();
            createInfo.attributeCount = (uint32_t)attributes.size();
            createInfo.pIndices = (uint8_t*)&colladaMesh->pTriangles->data[0];
            createInfo.indexCount = colladaMesh->triangleCount*3*createInfo.attributeCount;
            createInfo.indexTypeSize = sizeof(uint32_t);
            createInfo.flags = MO_VERTEX_FORMAT_INDICES_COUNT_FROM_ONE_BIT | MO_VERTEX_FORMAT_INDICES_PER_ATTRIBUTE_BIT | MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT;
            moCreateVertexFormat(&createInfo, &vertexFormat);

            MoMeshCreateInfo meshInfo = {};
            meshInfo.indexCount = vertexFormat->indexCount;
            meshInfo.pIndices = vertexFormat->pIndices;
            meshInfo.vertexCount = vertexFormat->vertexCount;
            meshInfo.pVertices = vertexFormat->pVertices;
            moCreateMesh(&meshInfo, &meshes[i]);
            colladaMesh->userData = meshes[i];

            moDestroyVertexFormat(vertexFormat);
        }

        // flatten transforms of node tree
        for (uint32_t i = 0; i < collada->nodeCount; ++i)
        {
            std::function<void(MoColladaNode, float4x4)> recurse = [&](MoColladaNode currentNode, float4x4 transform)
            {
                for (uint32_t j = 0; j < currentNode->nodeCount; ++j)
                {
                    MoColladaNode child = currentNode->pNodes[j];
                    recurse(child, mul(transform, (float4x4&)child->transform));
                }
                if (currentNode->mesh != nullptr)
                {
                    drawables.push_back(Drawable{transform, (MoMesh)currentNode->mesh->userData});
                }
            };
            MoColladaNode colladaNode = collada->pNodes[i];
            recurse(colladaNode, (float4x4&)colladaNode->transform);
        }

        moDestroyColladaData(collada);
    }
    // Demo
    if (meshes.empty()) { meshes.resize(1); meshes[0] = {}; moDemoCube(&meshes[0]); drawables.push_back(Drawable{model_matrix, meshes[0]}); }
    if (material == nullptr) { moDemoMaterial(&material); }

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
            MoUniform uni = {};
            (float3&)uni.light = light_position;
            (float3&)uni.camera = camera_matrix.w.xyz();
            moSetLight(&uni);
        }
        {
            MoPushConstant pmv = {};
            (float4x4&)pmv.projection = proj_matrix;
            (float4x4&)pmv.view = view_matrix;
            for (const auto & drawable : drawables)
            {
                (float4x4&)pmv.model = drawable.model;
                moSetPMV(&pmv);
                moDrawMesh(drawable.mesh);
            }
        }

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
    for (MoMesh mesh : meshes)
        moDestroyMesh(mesh);

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
