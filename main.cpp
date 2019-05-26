#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "collada.h"
#include "control.h"
#include "dome.h"
#include "gui.h"
#include "phong.h"
#include "vertexformat.h"

#include <linalg.h>
#include <lodepng.h>

#include <cstdio>
#include <cstdlib>
#include <experimental/filesystem>
#include <functional>
#include <fstream>
#include <random>
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
static float4x4 proj_matrix = mul(corr_matrix, perspective_matrix(degreesToRadians(75.f), 16/9.f, 0.1f, 1000.f, pos_z, zero_to_one));
static float4x4 camera_matrix = translation_matrix(float3{ 3.0f, 0.0f, 15.0f });
static float3   light_position = { -300.0f, 300.0f, -150.0f };
static MoMouselook mouselook;

static void glfwKeyCallback(GLFWwindow *window, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_ESCAPE)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        else if (key == GLFW_KEY_R)
        {
            moResetMouselook(mouselook);
        }
    }
}

int main(int argc, char** argv)
{
#ifndef NDEBUG
    moTestCollada();
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
        int width, height;

        glfwSetErrorCallback(glfw_error_callback);
        glfwInit();
#ifndef NDEBUG
        width = 1920 / 2;
        height = 1080 / 2;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, "Graphics Previewer", nullptr, nullptr);
#else
        {
            const GLFWvidmode * mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
            width = mode->width;
            height = mode->height;
        }
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        window = glfwCreateWindow(width, height, "Graphics Previewer", nullptr, nullptr);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetWindowMonitor(window, glfwGetPrimaryMonitor(), 0, 0, width, height, GLFW_DONT_CARE);
#endif
        glfwSetKeyCallback(window, glfwKeyCallback);
        if (!glfwVulkanSupported()) printf("GLFW: Vulkan Not Supported\n");

        // Create Vulkan instance
        {
            MoInstanceCreateInfo createInfo = {};
            createInfo.pExtensions = glfwGetRequiredInstanceExtensions(&createInfo.extensionsCount);
#ifndef NDEBUG
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
        width = height = 0;
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

    // imgui initialization
    {
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = instance;
        init_info.PhysicalDevice = device->physicalDevice;
        init_info.Device = device->device;
        init_info.QueueFamily = device->queueFamily;
        init_info.Queue = device->queue;
        init_info.PipelineCache = pipelineCache;
        init_info.DescriptorPool = device->descriptorPool;
        init_info.Allocator = allocator;
        init_info.MinImageCount = 2;
        init_info.ImageCount = 2;
        init_info.CheckVkResultFn = device->pCheckVkResultFn;

        // Use any command queue
        VkCommandPool commandPool = swapChain->frames[frameIndex].pool;
        VkCommandBuffer commandBuffer = swapChain->frames[frameIndex].buffer;

        moGUIInit(window, device->device, swapChain->renderPass, commandPool, commandBuffer, device->queue, &init_info);
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
        float4x4   model;
        MoMesh     mesh;
        MoMaterial material;
    };
    // reference only
    std::vector<Drawable> drawables;
    // owning
    std::vector<MoMesh> meshes;
    // owning
    std::vector<MoMaterial> materials;

    if (filename != nullptr)
    {
        MoColladaData collada;

        // the file
        {
            std::ifstream fileStream(filename);
            std::string contents((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
            MoColladaDataCreateInfo createInfo;
            createInfo.pContents = contents.data();
            moCreateColladaData(&createInfo, &collada);
        }

        // meshes
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
            createInfo.pIndices = (const uint8_t*)&colladaMesh->pTriangles->data[0];
            createInfo.indexCount = colladaMesh->triangleCount*3*createInfo.attributeCount;
            createInfo.indexTypeSize = sizeof(uint32_t);
            createInfo.flags = MO_VERTEX_FORMAT_INDICES_COUNT_FROM_ONE_BIT | MO_VERTEX_FORMAT_INDICES_PER_ATTRIBUTE_BIT | MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT;
            moCreateVertexFormat(&createInfo, &vertexFormat);

            MoMeshCreateInfo meshInfo = {};
            meshInfo.indexCount = vertexFormat->indexCount;
            meshInfo.pIndices = vertexFormat->pIndices;
            meshInfo.vertexCount = vertexFormat->vertexCount;
            meshInfo.pVertices = vertexFormat->pVertices;

            MoMesh mesh;
            moCreateMesh(&meshInfo, &mesh);
            colladaMesh->userData = mesh;
            meshes.push_back(mesh);

            moDestroyVertexFormat(vertexFormat);
        }

        // materials
        std::filesystem::path parentdirectory = std::filesystem::path(filename).parent_path();
        materials.push_back({}); materials.back() = {}; moDefaultMaterial(&materials.back());
        for (uint32_t i = 0; i < collada->materialCount; ++i)
        {
            MoColladaMaterial colladaMaterial = collada->pMaterials[i];

            MoMaterialCreateInfo materialInfo = {};
            (MoFloat3&)materialInfo.colorAmbient  = colladaMaterial->colorAmbient ; materialInfo.colorAmbient.w  = 1.0f;
            (MoFloat3&)materialInfo.colorDiffuse  = colladaMaterial->colorDiffuse ; materialInfo.colorDiffuse.w  = 1.0f;
            (MoFloat3&)materialInfo.colorSpecular = colladaMaterial->colorSpecular; materialInfo.colorSpecular.w = 1.0f;
            (MoFloat3&)materialInfo.colorEmissive = colladaMaterial->colorEmissive; materialInfo.colorEmissive.w = 1.0f;

            std::vector<uint8_t> dataDiffuse, dataNormal, dataSpecular, dataEmissive;
            if (colladaMaterial->filenameDiffuse  && std::filesystem::exists(parentdirectory / colladaMaterial->filenameDiffuse )) { lodepng::decode(dataDiffuse , materialInfo.textureDiffuse.extent.width, materialInfo.textureDiffuse.extent.height  , (parentdirectory / colladaMaterial->filenameDiffuse ).u8string()); materialInfo.textureDiffuse.pData  = dataDiffuse.data (); }
            if (colladaMaterial->filenameNormal   && std::filesystem::exists(parentdirectory / colladaMaterial->filenameNormal  )) { lodepng::decode(dataNormal  , materialInfo.textureNormal.extent.width, materialInfo.textureNormal.extent.height    , (parentdirectory / colladaMaterial->filenameNormal  ).u8string()); materialInfo.textureNormal.pData   = dataNormal.data  (); }
            if (colladaMaterial->filenameSpecular && std::filesystem::exists(parentdirectory / colladaMaterial->filenameSpecular)) { lodepng::decode(dataSpecular, materialInfo.textureSpecular.extent.width, materialInfo.textureSpecular.extent.height, (parentdirectory / colladaMaterial->filenameSpecular).u8string()); materialInfo.textureSpecular.pData = dataSpecular.data(); }
            if (colladaMaterial->filenameEmissive && std::filesystem::exists(parentdirectory / colladaMaterial->filenameEmissive)) { lodepng::decode(dataEmissive, materialInfo.textureEmissive.extent.width, materialInfo.textureEmissive.extent.height, (parentdirectory / colladaMaterial->filenameEmissive).u8string()); materialInfo.textureEmissive.pData = dataEmissive.data(); }

            MoMaterial material = {};
            moCreateMaterial(&materialInfo, &material);
            colladaMaterial->userData = material;
            materials.push_back(material);
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
                    if (currentNode->material != nullptr) { drawables.push_back(Drawable{transform, (MoMesh)currentNode->mesh->userData, (MoMaterial)currentNode->material->userData}); }
                    else                                  { drawables.push_back(Drawable{transform, (MoMesh)currentNode->mesh->userData, materials[0]}); }
                }
            };
            MoColladaNode colladaNode = collada->pNodes[i];
            recurse(colladaNode, (float4x4&)colladaNode->transform);
        }

        moDestroyColladaData(collada);
    }

    // Demo
    if (meshes.empty())
    {
        meshes.push_back({}); meshes.back() = {}; moDemoCube(&meshes.back());
        materials.push_back({}); materials.back() = {}; moDemoMaterial(&materials.back());

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dis(-10.0f, 10.0f);
        for (uint32_t i = 0; i < 16; ++i)
            drawables.push_back(Drawable{translation_matrix(float3{ dis(gen), dis(gen), dis(gen) }), meshes.back(), materials.back()});
    }

    // Dome
    MoMesh sphereMesh;
    moDemoSphere(&sphereMesh);
    MoMaterial domeMaterial;
    {
        MoMaterialCreateInfo materialInfo = {};
        materialInfo.colorAmbient = { 0.4f, 0.5f, 0.75f, 1.0f };
        materialInfo.colorDiffuse = { 0.7f, 0.45f, 0.1f, 1.0f };
        moCreateMaterial(&materialInfo, &domeMaterial);
    }
    MoPipeline domePipeline;
    {
        MoPipelineCreateInfo pipelineCreateInfo = {};
        pipelineCreateInfo.pVertexShader = mo_dome_glsl_shader_vert_spv;
        pipelineCreateInfo.vertexShaderSize = sizeof(mo_dome_glsl_shader_vert_spv);
        pipelineCreateInfo.pFragmentShader = mo_dome_glsl_shader_frag_spv;
        pipelineCreateInfo.fragmentShaderSize = sizeof(mo_dome_glsl_shader_frag_spv);
        pipelineCreateInfo.flags = MO_PIPELINE_FEATURE_NONE;
        moCreatePipeline(&pipelineCreateInfo, &domePipeline);
    }

    // Controls init
    float yaw, pitch; yaw = pitch = 0.f;
    {
        MoControlInitInfo initInfo = {};
        initInfo.pWindow = window;
        moControlInit(&initInfo);

        MoMouselookCreateInfo createInfo = {};
        createInfo.pPitch = &pitch;
        createInfo.pYaw = &yaw;
        moCreateMouselook(&createInfo, &mouselook);
    }
    MoStrafer strafer;
    bool w, a, s, d; w = a = s = d = false;
    {
        MoStraferCreateInfo createInfo = {};
        createInfo.pForward  = &w;
        createInfo.pLeft     = &a;
        createInfo.pBackward = &s;
        createInfo.pRight    = &d;
        moCreateStrafer(&createInfo, &strafer);
    }

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Camera control
        float4x4 camera_azimuth_matrix = rotation_matrix(rotation_quat({0.f,-1.f,0.f}, yaw * 0.01f));
        float4x4 camera_altitude_matrix = rotation_matrix(rotation_quat({-1.f,0.f,0.f}, pitch * 0.01f));
        float4 forward = mul(camera_azimuth_matrix, float4(0,0,1,0));
        float4 right = mul(camera_azimuth_matrix,   float4(1,0,0,0));
        float4 linearVelocity = {};
        if (w) linearVelocity.z = -3.3f;
        if (a) linearVelocity.x = -3.3f;
        if (s) linearVelocity.z = 3.3f;
        if (d) linearVelocity.x = 3.3f;
        camera_matrix.w += 0.016f * (linearVelocity.z * forward) + 0.016f * (linearVelocity.x * right);
        float4x4 camera_matrix_frame = mul(camera_matrix, mul(camera_azimuth_matrix, camera_altitude_matrix));

        // Frame begin
        VkSemaphore imageAcquiredSemaphore;
        moBeginSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);
        moPipelineOverride(domePipeline);
        moBegin(frameIndex);
        {
            MoUniform uni = {};
            (float3&)uni.light = light_position;
            (float3&)uni.camera = camera_matrix_frame.w.xyz();
            moSetLight(&uni);
        }
        {
            MoPushConstant pmv = {};
            (float4x4&)pmv.projection = proj_matrix;
            (float4x4&)pmv.view = mul(inverse(mul(camera_azimuth_matrix, camera_altitude_matrix)), rotation_matrix(rotation_quat(normalize(float3(-1.0f, -1.0f, 0.0f)), 355.0f/113.0f / 32.0f)));
            {
                (float4x4&)pmv.model = identity;
                moSetPMV(&pmv);
                moBindMaterial(domeMaterial);
                moDrawMesh(sphereMesh);
            }
        }
        moPipelineOverride();
        moBegin(frameIndex);
        {
            MoUniform uni = {};
            (float3&)uni.light = light_position;
            (float3&)uni.camera = camera_matrix_frame.w.xyz();
            moSetLight(&uni);
        }
        {
            MoPushConstant pmv = {};
            (float4x4&)pmv.projection = proj_matrix;
            (float4x4&)pmv.view = inverse(camera_matrix_frame);
            for (const auto & drawable : drawables)
            {
                (float4x4&)pmv.model = drawable.model;
                moSetPMV(&pmv);
                moBindMaterial(drawable.material);
                moDrawMesh(drawable.mesh);
            }
        }

        moGUIDraw(swapChain->frames[frameIndex].buffer);

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

            // in case the window was resized
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

    // Controls cleanup
    moDestroyStrafer(strafer);
    moDestroyMouselook(mouselook);
    moControlShutdown();

    // Dome
    moDestroyPipeline(domePipeline);
    moDestroyMaterial(domeMaterial);

    // Meshoui cleanup
    for (MoMaterial material : materials)
        moDestroyMaterial(material);
    moDestroyMesh(sphereMesh);
    for (MoMesh mesh : meshes)
        moDestroyMesh(mesh);

    // imgui cleanup
    moGUIShutdown();

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
