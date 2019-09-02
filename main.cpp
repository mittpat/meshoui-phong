#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#ifndef MO_HEADLESS
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#endif

#include "phong.h"
#include <lightmap.h>

#include <linalg.h>

#include <experimental/filesystem>
#include <functional>
#include <vector>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#include <iostream>
#include <fstream>

namespace std { namespace filesystem = experimental::filesystem; }
using namespace linalg;
using namespace linalg::aliases;

#ifndef MO_HEADLESS
static void glfw_error_callback(int, const char* description)
{
    printf("GLFW Error: %s\n", description);
}
#endif
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

#define MoPI (355.f/113)
static constexpr float moDegreesToRadians(float angle)
{
    return angle * MoPI / 180.0f;
}

static float4x4 correction_matrix = { { 1.0f, 0.0f, 0.0f, 0.0f },
                                      { 0.0f,-1.0f, 0.0f, 0.0f },
                                      { 0.0f, 0.0f, 1.0f, 0.0f },
                                      { 0.0f, 0.0f, 0.0f, 1.0f } };
static float4x4 projection_matrix = mul(correction_matrix, perspective_matrix(moDegreesToRadians(75.f), 16/9.f, 0.1f, 1000.f, pos_z, zero_to_one));
static float4x4 orthographic_matrix = mul(correction_matrix, frustum_matrix(-0.5f, 0.5f, -0.5f, 0.5f, 1.0f, 1000.f, neg_z, zero_to_one));
#ifndef MO_HEADLESS
struct MoInputs
{
    bool up;
    bool down;
    bool left;
    bool right;
    bool forward;
    bool backward;
    bool leftButton;
    bool rightButton;
    double xpos, ypos;
    double dxpos, dypos;
};

static void glfwKeyCallback(GLFWwindow *window, int key, int /*scancode*/, int action, int /*mods*/)
{
    MoInputs* inputs = (MoInputs*)glfwGetWindowUserPointer(window);
    if (action == GLFW_PRESS)
    {
        switch (key)
        {
        case GLFW_KEY_ESCAPE:
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_Q:
            inputs->up = true;
            break;
        case GLFW_KEY_E:
            inputs->down = true;
            break;
        case GLFW_KEY_W:
            inputs->forward = true;
            break;
        case GLFW_KEY_A:
            inputs->left = true;
            break;
        case GLFW_KEY_S:
            inputs->backward = true;
            break;
        case GLFW_KEY_D:
            inputs->right = true;
            break;
        }
    }
    if (action == GLFW_RELEASE)
    {
        switch (key)
        {
        case GLFW_KEY_Q:
            inputs->up = false;
            break;
        case GLFW_KEY_E:
            inputs->down = false;
            break;
        case GLFW_KEY_W:
            inputs->forward = false;
            break;
        case GLFW_KEY_A:
            inputs->left = false;
            break;
        case GLFW_KEY_S:
            inputs->backward = false;
            break;
        case GLFW_KEY_D:
            inputs->right = false;
            break;
        }
    }
}

static void glfwMouseCallback(GLFWwindow *window, int button, int action, int /*mods*/)
{
    MoInputs* inputs = (MoInputs*)glfwGetWindowUserPointer(window);
    if (action == GLFW_PRESS)
    {
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            inputs->leftButton = true;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            inputs->rightButton = true;
            break;
        }
    }
    else if (action == GLFW_RELEASE)
    {
        switch (button)
        {
        case GLFW_MOUSE_BUTTON_LEFT:
            inputs->leftButton = false;
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            inputs->rightButton = false;
            break;
        }
    }
}

static void glfwPollMouse(GLFWwindow *window)
{
    MoInputs* inputs = (MoInputs*)glfwGetWindowUserPointer(window);

    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    inputs->dxpos = xpos - inputs->xpos;
    inputs->dypos = ypos - inputs->ypos;
    inputs->xpos = xpos;
    inputs->ypos = ypos;
}
#endif

struct MoLight
{
    std::string name;
    float4x4    model;
    float       power;
};

struct MoCamera
{
    std::string name;
    float3      position;
    float       pitch, yaw;
    float4x4    model()
    {
        float3 right = mul(rotation_matrix(rotation_quat({0.f,-1.f,0.f}, yaw)), {1.f,0.f,0.f,0.f}).xyz();

        return mul(translation_matrix(position), mul(rotation_matrix(rotation_quat(right, pitch)), rotation_matrix(rotation_quat({0.f,-1.f,0.f}, yaw))));
    }
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
        std::vector<MoTextureSample> output(256*256, {0,0,0,0});

        Assimp::Importer importer;
        const aiScene * scene = importer.ReadFile(filename, aiProcess_Debone | aiProcessPreset_TargetRealtime_Fast);
        std::filesystem::path parentdirectory = std::filesystem::path(filename).parent_path();

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

            //MoMeshCreateInfo meshInfo = {};
            //meshInfo.indexCount = mesh->mNumFaces * 3;
            //meshInfo.pIndices = indices.data();
            //meshInfo.vertexCount = mesh->mNumVertices;
            //meshInfo.pVertices = vertices.data();
            //meshInfo.pTextureCoords = textureCoords.data();
            //meshInfo.pNormals = normals.data();
            //meshInfo.pTangents = tangents.data();
            //meshInfo.pBitangents = bitangents.data();
            linalg::aliases::float2 halfExtents = linalg::aliases::float2(0.5f, 0.5f);
            {
                static float3 cube_positions[] = { { -halfExtents.x, -halfExtents.y, 0 },
                                                   { -halfExtents.x, -halfExtents.y, 0 },
                                                   { -halfExtents.x,  halfExtents.y, 0 },
                                                   { -halfExtents.x,  halfExtents.y, 0 },
                                                   {  halfExtents.x, -halfExtents.y, 0 },
                                                   {  halfExtents.x, -halfExtents.y, 0 },
                                                   {  halfExtents.x,  halfExtents.y, 0 },
                                                   {  halfExtents.x,  halfExtents.y, 0 } };
                static float2 cube_texcoords[] = { { 2 * halfExtents.x, 0.0f },
                                                   { 0.0f, 2 * halfExtents.x },
                                                   { 0.0f, 0.0f },
                                                   { 2 * halfExtents.x, 2 * halfExtents.x } };
                static float3 cube_normals[] = { { 0.0f, 0.0f, 1.0f } };
                static mat<unsigned,3,3> cube_triangles[] = { //{ uint3{ 2, 3, 1 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                              //{ uint3{ 4, 7, 3 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                              //{ uint3{ 8, 5, 7 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                              //{ uint3{ 6, 1, 5 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                              //{ uint3{ 7, 1, 3 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                              { uint3{ 4, 6, 8 }, uint3{ 1, 2, 3 }, uint3{ 1, 1, 1 } },
                                                              //{ uint3{ 2, 4, 3 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                              //{ uint3{ 4, 8, 7 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                              //{ uint3{ 8, 6, 5 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                              //{ uint3{ 6, 2, 1 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
                                                              //{ uint3{ 7, 5, 1 }, uint3{ 1, 4, 2 }, uint3{ 1, 1, 1 } },
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
                meshInfo.name = mesh->mName.C_Str();

                MoTriangleList triangleList;
                moCreateTriangleList(mesh, &triangleList);
                meshInfo.bvh = triangleList->bvh;

                moCreateMesh(&meshInfo, &meshes[meshIdx]);
                handles.meshes.push_back(meshes[meshIdx]);
            }


            //MoTriangleList triangleList;
            //moCreateTriangleList(mesh, &triangleList);
            //meshInfo.bvh = triangleList->bvh;

            //moCreateMesh(&meshInfo, &meshes[meshIdx]);
            //handles.meshes.push_back(meshes[meshIdx]);

#if 0
            {
                MoTriangleList triangleList;
                moCreateTriangleList(mesh, &triangleList);

                MoLightmapCreateInfo info = {};
                info.nullColor = {127,127,127,255};
                info.size = {256,256};
                info.flipY = 1;
                info.despeckle = 1;
                info.ambientLightingSampleCount = 512;
                info.ambientLightingPower = 1.0f;
                info.ambientOcclusionDistance = 1.f;
                info.directionalLightingSampleCount = 32;
                info.pDirectionalLightSources = nullptr;
                info.directionalLightSourceCount = 0;
                info.pointLightingSampleCount = 32;
                info.pPointLightSources = nullptr;
                info.pointLightSourceCount = 0;
                moGenerateLightMap(triangleList, output.data(), &info, &std::cout);
                moDestroyTriangleList(triangleList);
            }
#endif
        }

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
                auto t = material->GetTextureCount(mapping.first);
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

            // AO
            materialInfo.textureAmbient.pData = (const uint8_t*)output.data();
            materialInfo.textureAmbient.extent = {256,256};
            materialInfo.textureAmbient.filter = VK_FILTER_LINEAR;
            materialInfo.name = material->GetName().C_Str();

            moCreateMaterial(&materialInfo, &materials[materialIdx]);
            handles.materials.push_back(materials[materialIdx]);
        }

        nodes.push_back({std::filesystem::canonical(filename).c_str(), identity,
                         nullptr, nullptr, {}});
        parseNodes(scene, materials, meshes, scene->mRootNode, nodes.back().children);
    }
}

int main(int argc, char** argv)
{
#ifndef MO_HEADLESS
    GLFWwindow*                  window = nullptr;
#endif
    VkInstance                   instance = VK_NULL_HANDLE;
    MoDevice                     device = VK_NULL_HANDLE;
    MoSwapChain                  swapChain = VK_NULL_HANDLE;
    VkSurfaceKHR                 surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR           surfaceFormat = {};
    uint32_t                     frameIndex = 0;
    VkPipelineCache              pipelineCache = VK_NULL_HANDLE;
    const VkAllocationCallbacks* allocator = VK_NULL_HANDLE;
#ifndef MO_HEADLESS
    MoInputs                     inputs = {};
#endif
    // Initialization
    {
        int width, height;
#undef NDEBUG
#ifndef NDEBUG
        width = 1920 / 2;
        height = 1080 / 2;
#endif
#ifndef MO_HEADLESS
        glfwSetErrorCallback(glfw_error_callback);
        glfwInit();
#ifndef NDEBUG
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
        glfwGetCursorPos(window, &inputs.xpos, &inputs.ypos);
        glfwSetWindowUserPointer(window, &inputs);
        glfwSetKeyCallback(window, glfwKeyCallback);
        glfwSetMouseButtonCallback(window, glfwMouseCallback);
        if (!glfwVulkanSupported()) printf("GLFW: Vulkan Not Supported\n");
#endif
        // Create Vulkan instance
        {
            MoInstanceCreateInfo createInfo = {};
#ifndef MO_HEADLESS
            createInfo.pExtensions = glfwGetRequiredInstanceExtensions(&createInfo.extensionsCount);
#endif
#ifndef NDEBUG
            createInfo.debugReport = VK_TRUE;
            createInfo.pDebugReportCallback = vk_debug_report;
#endif
            createInfo.pCheckVkResultFn = vk_check_result;
            moCreateInstance(&createInfo, &instance);
        }

        // Create Window Surface
        VkResult err = VK_SUCCESS;
#ifndef MO_HEADLESS
        err = glfwCreateWindowSurface(instance, window, allocator, &surface);
        vk_check_result(err);

        // Create Framebuffers
        width = height = 0;
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwPostEmptyEvent();
            glfwWaitEvents();
        }
#endif
        projection_matrix = mul(correction_matrix, perspective_matrix(moDegreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

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
#ifndef MO_HEADLESS
        initInfo.pSwapChainSwapBuffers = swapChain->images;
        initInfo.swapChainSwapBufferCount = MO_FRAME_COUNT;
#endif
        initInfo.pSwapChainCommandBuffers = swapChain->frames;
        initInfo.swapChainCommandBufferCount = MO_FRAME_COUNT;
        initInfo.depthBuffer = swapChain->depthBuffer;
#ifndef MO_HEADLESS
        initInfo.swapChainKHR = swapChain->swapChainKHR;
#endif
        initInfo.renderPass = swapChain->renderPass;
        initInfo.extent = swapChain->extent;
        initInfo.pAllocator = allocator;
        initInfo.pCheckVkResultFn = device->pCheckVkResultFn;
        moInit(&initInfo);
    }

    MoHandles handles;
    MoNode root{"__root", identity, nullptr, nullptr, {}};
    MoCamera camera{"__default_camera", {0.f, 10.f, 30.f}, 0.f, 0.f};
    MoLight light{"__default_light", translation_matrix(float3{-300.f, 300.f, 150.f}), 0.f};

    std::filesystem::path fileToLoad = "teapot.dae";


    // Rect
    MoMesh rectMesh;
    moDemoPlane(&rectMesh);
    MoMaterial rectMaterial;
    moDemoMaterial(&rectMaterial);

    // Dome
    MoMesh sphereMesh;
    moDemoSphere(&sphereMesh);
    MoMaterial domeMaterial;
    {
        MoMaterialCreateInfo materialInfo = {};
        materialInfo.colorAmbient = { 0.4f, 0.5f, 0.75f, 1.0f };
        materialInfo.colorDiffuse = { 0.7f, 0.45f, 0.1f, 1.0f };
        materialInfo.name = "DomeMaterial";
        moCreateMaterial(&materialInfo, &domeMaterial);
    }
    MoPipeline domePipeline;
    {
        MoPipelineCreateInfo pipelineCreateInfo = {};
        std::vector<char> mo_dome_shader_vert_spv;
        {
            std::ifstream fileStream("dome.vert.spv", std::ifstream::binary);
            mo_dome_shader_vert_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
        }
        std::vector<char> mo_dome_shader_frag_spv;
        {
            std::ifstream fileStream("dome.frag.spv", std::ifstream::binary);
            mo_dome_shader_frag_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
        }
        pipelineCreateInfo.pVertexShader = (std::uint32_t*)mo_dome_shader_vert_spv.data();
        pipelineCreateInfo.vertexShaderSize = mo_dome_shader_vert_spv.size();
        pipelineCreateInfo.pFragmentShader = (std::uint32_t*)mo_dome_shader_frag_spv.data();
        pipelineCreateInfo.fragmentShaderSize = mo_dome_shader_frag_spv.size();
        pipelineCreateInfo.flags = MO_PIPELINE_FEATURE_NONE;
        pipelineCreateInfo.name = "Dome";
        moCreatePipeline(&pipelineCreateInfo, &domePipeline);
    }

    // Main loop
#ifndef MO_HEADLESS
    while (!glfwWindowShouldClose(window))
#endif
    {
#ifndef MO_HEADLESS
        glfwPollEvents();
        glfwPollMouse(window);
#endif
        if (!fileToLoad.empty())
        {
            load(fileToLoad, handles, root.children);
            fileToLoad = "";
        }
#ifndef MO_HEADLESS
        {
            const float speed = 0.5f;
            float3 forward = mul(camera.model(), {0.f,0.f,-1.f,0.f}).xyz();
            float3 up = mul(camera.model(), {0.f,1.f,0.f,0.f}).xyz();
            float3 right = mul(camera.model(), {1.f,0.f,0.f,0.f}).xyz();
            if (inputs.up) camera.position += up * speed;
            if (inputs.down) camera.position -= up * speed;
            if (inputs.forward) camera.position += forward * speed;
            if (inputs.backward) camera.position -= forward * speed;
            if (inputs.left) camera.position -= right * speed;
            if (inputs.right) camera.position += right * speed;
            if (inputs.leftButton)
            {
                camera.yaw += inputs.dxpos * 0.005;
                camera.pitch -= inputs.dypos * 0.005;
            }
        }
#endif
        // Frame begin
        VkSemaphore imageAcquiredSemaphore;
        moBeginSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);
        moPipelineOverride(domePipeline);
        moBegin(frameIndex);

        {
            MoUniform uni = {};
            uni.light = float4(light.model.w.xyz(), light.power);
            uni.camera = camera.position;
            moSetLight(&uni);
        }
        {
            float4x4 view = inverse(camera.model());
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
            uni.light = float4(light.model.w.xyz(), light.power);
            uni.camera = camera.model().w.xyz();
            moSetLight(&uni);
        }
#if 0
        {
            MoPushConstant pmv = {};
            pmv.projection = projection_matrix;
            pmv.view = inverse(camera.model());
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
#elif 1
        {
            camera = {"__default_camera", {0.f, 0.f, 1.f}, 0.f, 0.f};
            MoPushConstant pmv = {};
            pmv.projection = orthographic_matrix;
            pmv.view = inverse(camera.model());
            pmv.model = identity;
            std::function<void(const MoNode &, const float4x4 &)> draw = [&](const MoNode & node, const float4x4 & model)
            {
                if (node.material && node.mesh)
                {
                    moBindMaterial(rectMaterial);
                    //pmv.model = model;
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
#else
        {
            camera = {"__default_camera", {0.f, 0.f, 1.f}, 0.f, 0.f};

            MoPushConstant pmv = {};
            pmv.projection = orthographic_matrix;
            pmv.view = inverse(camera.model());
            pmv.model = identity;
            moSetPMV(&pmv);
            moBindMaterial(rectMaterial);
            moDrawMesh(rectMesh);
        }
#endif
        // Frame end
        VkResult err = moEndSwapChain(swapChain, &frameIndex, &imageAcquiredSemaphore);
#ifndef MO_HEADLESS
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
            projection_matrix = mul(correction_matrix, perspective_matrix(moDegreesToRadians(75.f), width / float(height), 0.1f, 1000.f, neg_z, zero_to_one));

            MoSwapChainRecreateInfo recreateInfo = {};
            recreateInfo.surface = surface;
            recreateInfo.surfaceFormat = surfaceFormat;
            recreateInfo.extent = {(uint32_t)width, (uint32_t)height};
            recreateInfo.vsync = VK_TRUE;
            moRecreateSwapChain(&recreateInfo, swapChain);
            err = VK_SUCCESS;
        }
#endif
        vk_check_result(err);
    }



#ifndef MO_HEADLESS
#else
    const char* imagedata = {};
    {
        auto memoryType = [](VkPhysicalDevice physicalDevice, VkMemoryPropertyFlags properties, uint32_t type_bits) -> uint32_t
        {
            VkPhysicalDeviceMemoryProperties prop;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &prop);
            for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
                if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1<<i))
                    return i;
            return 0xFFFFFFFF;
        };

        // Create the linear tiled destination image to copy to and to read the memory from
        VkImageCreateInfo imgCreateInfo = {};
        imgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imgCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imgCreateInfo.extent.width = swapChain->extent.width;
        imgCreateInfo.extent.height = swapChain->extent.height;
        imgCreateInfo.extent.depth = 1;
        imgCreateInfo.arrayLayers = 1;
        imgCreateInfo.mipLevels = 1;
        imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
        imgCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        // Create the image
        VkImage dstImage;
        VkResult err = vkCreateImage(device->device, &imgCreateInfo, nullptr, &dstImage);
        device->pCheckVkResultFn(err);
        // Create memory to back up the image
        VkMemoryRequirements memRequirements;
        VkMemoryAllocateInfo memAllocInfo = {};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        VkDeviceMemory dstImageMemory;
        vkGetImageMemoryRequirements(device->device, dstImage, &memRequirements);
        memAllocInfo.allocationSize = memRequirements.size;
        // Memory must be host visible to copy from
        memAllocInfo.memoryTypeIndex = memoryType(device->physicalDevice, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, memRequirements.memoryTypeBits);
        err = vkAllocateMemory(device->device, &memAllocInfo, nullptr, &dstImageMemory);
        device->pCheckVkResultFn(err);
        err = vkBindImageMemory(device->device, dstImage, dstImageMemory, 0);
        device->pCheckVkResultFn(err);

        // Do the actual blit from the offscreen image to our host visible destination image
        VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
        cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufAllocateInfo.commandPool = swapChain->frames[0].pool;
        cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufAllocateInfo.commandBufferCount = 1;
        VkCommandBuffer copyCmd;
        err = vkAllocateCommandBuffers(device->device, &cmdBufAllocateInfo, &copyCmd);
        device->pCheckVkResultFn(err);
        VkCommandBufferBeginInfo cmdBufInfo = {};
        cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        err = vkBeginCommandBuffer(copyCmd, &cmdBufInfo);
        device->pCheckVkResultFn(err);

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
        imageCopyRegion.extent.width = swapChain->extent.width;
        imageCopyRegion.extent.height = swapChain->extent.height;
        imageCopyRegion.extent.depth = 1;

        vkCmdCopyImage(
            copyCmd,
            swapChain->images[0].back, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
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
        device->pCheckVkResultFn(err);

        {
            VkSubmitInfo submitInfo = {};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &copyCmd;
            VkFenceCreateInfo fenceInfo = {};
            fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            VkFence fence;
            res = vkCreateFence(device->device, &fenceInfo, nullptr, &fence);
            device->pCheckVkResultFn(err);
            res = vkQueueSubmit(device->queue, 1, &submitInfo, fence);
            device->pCheckVkResultFn(err);
            res = vkWaitForFences(device->device, 1, &fence, VK_TRUE, UINT64_MAX);
            device->pCheckVkResultFn(err);
            vkDestroyFence(device->device, fence, nullptr);
        }

        // Get layout of the image (including row pitch)
        VkImageSubresource subResource{};
        subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        VkSubresourceLayout subResourceLayout;

        vkGetImageSubresourceLayout(device->device, dstImage, &subResource, &subResourceLayout);

        // Map image memory so we can start copying from it
        vkMapMemory(device->device, dstImageMemory, 0, VK_WHOLE_SIZE, 0, (void**)&imagedata);
        imagedata += subResourceLayout.offset;

        /*
            Save host visible framebuffer image to disk (ppm format)
        */

            const char* filename = "headless.ppm";

            std::ofstream file(filename, std::ios::out | std::ios::binary);

            // ppm header
            file << "P6\n" << swapChain->extent.width << "\n" << swapChain->extent.height << "\n" << 255 << "\n";

            // If source is BGR (destination is always RGB) and we can't use blit (which does automatic conversion), we'll have to manually swizzle color components
            // Check if source is BGR and needs swizzle
            std::vector<VkFormat> formatsBGR = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SNORM };
            const bool colorSwizzle = (std::find(formatsBGR.begin(), formatsBGR.end(), VK_FORMAT_B8G8R8A8_UNORM) != formatsBGR.end());

            // ppm binary pixel data
            for (int32_t y = 0; y < swapChain->extent.height; y++) {
                unsigned int *row = (unsigned int*)imagedata;
                for (int32_t x = 0; x < swapChain->extent.width; x++) {
                    if (colorSwizzle) {
                        file.write((char*)row + 2, 1);
                        file.write((char*)row + 1, 1);
                        file.write((char*)row, 1);
                    }
                    else {
                        file.write((char*)row, 3);
                    }
                    row++;
                }
                imagedata += subResourceLayout.rowPitch;
            }
            file.close();

            printf("Framebuffer image saved to %s\n", filename);

            // Clean up resources
            vkUnmapMemory(device->device, dstImageMemory);
            vkFreeMemory(device->device, dstImageMemory, nullptr);
            vkDestroyImage(device->device, dstImage, nullptr);
    }
#endif


    // Dome
    moDestroyPipeline(domePipeline);
    moDestroyMaterial(domeMaterial);

    // Meshoui cleanup
    moDestroyHandles(handles);
    moDestroyMesh(sphereMesh);

    // Cleanup
    moShutdown();
    moDestroySwapChain(device, swapChain);
#ifndef MO_HEADLESS
    vkDestroySurfaceKHR(instance, surface, allocator);
#endif
    moDestroyDevice(device);
    moDestroyInstance(instance);
#ifndef MO_HEADLESS
    glfwDestroyWindow(window);
    glfwTerminate();
#endif
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
