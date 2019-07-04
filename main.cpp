#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "dome.h"
#include "phong.h"

#include <linalg.h>

#include <experimental/filesystem>
#include <functional>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

namespace std { namespace filesystem = experimental::filesystem; }
using namespace linalg;
using namespace linalg::aliases;

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

static float4x4 correction_matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
                                      { 0.0f,-1.0f, 0.0f, 0.0f },
                                      { 0.0f, 0.0f, 1.0f, 0.0f },
                                      { 0.0f, 0.0f, 0.0f, 1.0f } };
static float4x4 projection_matrix = mul(correction_matrix, perspective_matrix(degreesToRadians(75.f), 16/9.f, 0.1f, 1000.f, pos_z, zero_to_one));

static void glfwKeyCallback(GLFWwindow *window, int key, int /*scancode*/, int action, int /*mods*/)
{
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_ESCAPE)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
}

static void glfwMouseCallback(GLFWwindow */*window*/, int button, int action, int /*mods*/)
{
    if (action == GLFW_PRESS)
    {
        if (button == GLFW_MOUSE_BUTTON_LEFT)
        {
            // move the camera
        }
    }
}

struct MoLight
{
    std::string name;
    float4x4    model;
};

struct MoCamera
{
    std::string name;
    float4x4    model;
};

struct MoNode
{
    std::string         name;
    float4x4            model;
    MoMesh              mesh;
    MoMaterial          material;
    std::vector<MoNode> children;
};

struct MoHandles
{
    std::vector<MoMaterial> materials;
    std::vector<MoMesh>     meshes;
};

void moDestroyHandles(MoHandles & handles)
{
    for (auto material : handles.materials)
        moDestroyMaterial(material);
    for (auto mesh : handles.meshes)
        moDestroyMesh(mesh);
}

void parseNodes(const aiScene * scene, const std::vector<MoMaterial> & materials,
                const std::vector<MoMesh> & meshes, aiNode * node, std::vector<MoNode> & nodes)
{
    nodes.push_back({node->mName.C_Str(), transpose(float4x4((float*)&node->mTransformation)),
                     nullptr, nullptr, {}});
    for (uint32_t i = 0; i < node->mNumMeshes; ++i)
    {
        nodes.back().children.push_back({scene->mMeshes[node->mMeshes[i]]->mName.C_Str(), identity,
                                    meshes[node->mMeshes[i]],
                                    materials[scene->mMeshes[node->mMeshes[i]]->mMaterialIndex],
                                    {}});
    }

    for (uint32_t i = 0; i < node->mNumChildren; ++i)
    {
        parseNodes(scene, materials, meshes, node->mChildren[i], nodes.back().children);
    }
}

void load(const std::string & filename, MoHandles & handles, std::vector<MoNode> & nodes)
{
    if (!filename.empty() && std::filesystem::exists(filename))
    {
        Assimp::Importer importer;
        const aiScene * scene = importer.ReadFile(filename, aiProcess_Debone | aiProcessPreset_TargetRealtime_Fast);
        std::filesystem::path parentdirectory = std::filesystem::path(filename).parent_path();

        std::vector<MoMaterial> materials(scene->mNumMaterials);
        for (uint32_t materialIdx = 0; materialIdx < scene->mNumMaterials; ++materialIdx)
        {
            auto* material = scene->mMaterials[materialIdx];

            MoMaterialCreateInfo materialInfo = {};
            std::vector<std::pair<const char*, float4*>> colorMappings =
               {{std::array<const char*,3>{AI_MATKEY_COLOR_AMBIENT}[0], &materialInfo.colorAmbient},
                {std::array<const char*,3>{AI_MATKEY_COLOR_DIFFUSE}[0], &materialInfo.colorDiffuse},
                {std::array<const char*,3>{AI_MATKEY_COLOR_SPECULAR}[0], &materialInfo.colorSpecular},
                {std::array<const char*,3>{AI_MATKEY_COLOR_EMISSIVE}[0], &materialInfo.colorEmissive}};
            for (auto mapping : colorMappings)
            {
                aiColor3D color(0.f,0.f,0.f);
                scene->mMaterials[materialIdx]->Get(mapping.first, 0, 0, color);
                *mapping.second = {color.r, color.g, color.b, 1.0f};
            }

            std::vector<std::pair<aiTextureType, MoTextureInfo*>> textureMappings =
                {{aiTextureType_AMBIENT, &materialInfo.textureAmbient},
                {aiTextureType_DIFFUSE, &materialInfo.textureDiffuse},
                {aiTextureType_SPECULAR, &materialInfo.textureSpecular},
                {aiTextureType_EMISSIVE, &materialInfo.textureEmissive},
                {aiTextureType_NORMALS, &materialInfo.textureNormal}};
            for (auto mapping : textureMappings)
            {
                aiString path;
                if (AI_SUCCESS == material->GetTexture(mapping.first, 0, &path))
                {
                    std::filesystem::path filename = parentdirectory / path.C_Str();
                    if (std::filesystem::exists(filename))
                    {
                        int x, y, n;
                        mapping.second->pData = stbi_load(filename.c_str(), &x, &y, &n, STBI_rgb_alpha);
                        mapping.second->extent = {(uint32_t)x, (uint32_t)y};
                    }
                }
            }
            moCreateMaterial(&materialInfo, &materials[materialIdx]);
            handles.materials.push_back(materials[materialIdx]);
        }

        std::vector<MoMesh> meshes(scene->mNumMeshes);
        for (uint32_t meshIdx = 0; meshIdx < scene->mNumMeshes; ++meshIdx)
        {
            const auto* mesh = scene->mMeshes[meshIdx];

            std::vector<uint32_t> indices;
            for (uint32_t faceIdx = 0; faceIdx < mesh->mNumFaces; ++faceIdx)
            {
                const auto* face = &mesh->mFaces[faceIdx];
                switch(face->mNumIndices)
                {
                case 1:
                    break;
                case 2:
                    break;
                case 3:
                {
                    for(uint32_t numIndex = 0; numIndex < face->mNumIndices; numIndex++)
                    {
                        indices.push_back(face->mIndices[numIndex]);
                    }
                    break;
                }
                default:
                    break;
                }
            }

            std::vector<float3> vertices, normals, tangents, bitangents;
            std::vector<float2> textureCoords;
            vertices.resize(mesh->mNumVertices);
            textureCoords.resize(mesh->mNumVertices);
            normals.resize(mesh->mNumVertices);
            tangents.resize(mesh->mNumVertices);
            bitangents.resize(mesh->mNumVertices);
            for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex)
            {
                vertices[vertexIndex] = float3((float*)&mesh->mVertices[vertexIndex]);
                if (mesh->HasTextureCoords(0)) { textureCoords[vertexIndex] = float2((float*)&mesh->mTextureCoords[0][vertexIndex]); }
                if (mesh->mNormals) { normals[vertexIndex] = float3((float*)&mesh->mNormals[vertexIndex]); }
                if (mesh->mTangents) { tangents[vertexIndex] = float3((float*)&mesh->mTangents[vertexIndex]); }
                if (mesh->mBitangents) { bitangents[vertexIndex] = float3((float*)&mesh->mBitangents[vertexIndex]); }
            }

            MoMeshCreateInfo meshInfo = {};
            meshInfo.indexCount = mesh->mNumFaces * 3;
            meshInfo.pIndices = indices.data();
            meshInfo.vertexCount = mesh->mNumVertices;
            meshInfo.pVertices = vertices.data();
            meshInfo.pTextureCoords = textureCoords.data();
            meshInfo.pNormals = normals.data();
            meshInfo.pTangents = tangents.data();
            meshInfo.pBitangents = bitangents.data();
            moCreateMesh(&meshInfo, &meshes[meshIdx]);
            handles.meshes.push_back(meshes[meshIdx]);
        }

        nodes.push_back({std::filesystem::canonical(filename).c_str(), identity,
                         nullptr, nullptr, {}});
        parseNodes(scene, materials, meshes, scene->mRootNode, nodes.back().children);
    }
}

int main(int argc, char** argv)
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
        glfwSetMouseButtonCallback(window, glfwMouseCallback);
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

        projection_matrix = mul(correction_matrix, perspective_matrix(degreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

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
        initInfo.flipTexcoords = VK_FALSE;//TRUE;
        moInit(&initInfo);
    }

    MoHandles handles;
    MoNode root{"__root", identity, nullptr, nullptr, {}};
    MoCamera camera{"__default_camera", translation_matrix(float3{0.0f, 0.0f, 50.0f})};
    MoLight light{"__default_light", translation_matrix(float3{-300.0f, 300.0f, 150.0f})};

    std::filesystem::path fileToLoad = "teapot.dae";

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

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (!fileToLoad.empty())
        {
            load(fileToLoad, handles, root.children);
            fileToLoad = "";
        }

        // Frame begin
        VkSemaphore imageAcquiredSemaphore;
        moBeginSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);
        moPipelineOverride(domePipeline);
        moBegin(frameIndex);

        {
            MoUniform uni = {};
            uni.light = light.model.w.xyz();
            uni.camera = camera.model.w.xyz();
            moSetLight(&uni);
        }
        {
            float4x4 view = inverse(camera.model);
            view.w = float4(0,0,0,1);

            MoPushConstant pmv = {};
            pmv.projection = projection_matrix;
            pmv.view = view;
            {
                pmv.model = identity;
                moSetPMV(&pmv);
                moBindMaterial(domeMaterial);
                moDrawMesh(sphereMesh);
            }
        }
        moPipelineOverride();
        moBegin(frameIndex);
        {
            MoUniform uni = {};
            uni.light = light.model.w.xyz();
            uni.camera = camera.model.w.xyz();
            moSetLight(&uni);
        }
        {
            MoPushConstant pmv = {};
            pmv.projection = projection_matrix;
            pmv.view = inverse(camera.model);
            std::function<void(const MoNode &, const float4x4 &)> draw = [&](const MoNode & node, const float4x4 & model)
            {
                if (node.material && node.mesh)
                {
                    moBindMaterial(node.material);
                    pmv.model = model;
                    moSetPMV(&pmv);
                    moDrawMesh(node.mesh);
                }
                for (const MoNode & child : node.children)
                {
                    draw(child, mul(model, child.model));
                }
            };
            draw(root, root.model);
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

            // in case the window was resized
            projection_matrix = mul(correction_matrix, perspective_matrix(degreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

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

    // Dome
    moDestroyPipeline(domePipeline);
    moDestroyMaterial(domeMaterial);

    // Meshoui cleanup
    moDestroyHandles(handles);
    moDestroyMesh(sphereMesh);

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
