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
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define MO_SAVE_TO_FILE

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
                meshInfo.bvhUV = triangleList->bvhUV;

                moCreateMesh(&meshInfo, &meshes[meshIdx]);
                handles.meshes.push_back(meshes[meshIdx]);
            }
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
    const char * filename = "teapot.dae";
    std::filesystem::path fileToLoad = filename;

    std::cout << "generating light map for " << fileToLoad << std::endl;
    auto start = std::chrono::steady_clock::now();

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
    int width, height;
    width = 1024;
    height = 1024;
    {
//#undef NDEBUG
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

    // Dome
    MoPipeline raytracePipeline;
    {
        MoPipelineCreateInfo pipelineCreateInfo = {};
        std::vector<char> mo_raytrace_shader_vert_spv;
        {
            std::ifstream fileStream("raytrace.vert.spv", std::ifstream::binary);
            mo_raytrace_shader_vert_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
        }
        std::vector<char> mo_raytrace_shader_frag_spv;
        {
            std::ifstream fileStream("raytrace.frag.spv", std::ifstream::binary);
            mo_raytrace_shader_frag_spv = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
        }
        pipelineCreateInfo.pVertexShader = (std::uint32_t*)mo_raytrace_shader_vert_spv.data();
        pipelineCreateInfo.vertexShaderSize = mo_raytrace_shader_vert_spv.size();
        pipelineCreateInfo.pFragmentShader = (std::uint32_t*)mo_raytrace_shader_frag_spv.data();
        pipelineCreateInfo.fragmentShaderSize = mo_raytrace_shader_frag_spv.size();
        pipelineCreateInfo.flags = MO_PIPELINE_FEATURE_NONE;
        pipelineCreateInfo.name = "Raytrace";
        moCreatePipeline(&pipelineCreateInfo, &raytracePipeline);
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
#if 0
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
        moPipelineOverride(raytracePipeline);
        moBegin(frameIndex);
        {
            MoCamera camera = {"__default_camera", {0.f, 0.f, 1.f}, 0.f, 0.f};
            MoPushConstant pmv = {};
            pmv.projection = orthographic_matrix;
            pmv.view = inverse(camera.model());
            pmv.model = identity;
            moSetPMV(&pmv);
            std::function<void(const MoNode &)> draw = [&](const MoNode & node)
            {
                if (node.material && node.mesh)
                {
                    moDrawMesh(node.mesh);
                }
                for (const MoNode & child : node.children)
                {
                    draw(child);
                }
            };
            draw(root);
        }
        moPipelineOverride();
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
    std::vector<std::uint8_t> readback(width * height * 4);
    moFramebufferReadback(swapChain->images[0].back, {std::uint32_t(width), std::uint32_t(height)}, readback.data(), readback.size(), swapChain->frames[0].pool);
#endif

    // Dome
    moDestroyPipeline(raytracePipeline);

    // Meshoui cleanup
    moDestroyHandles(handles);

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

#ifdef MO_SAVE_TO_FILE
    {
        std::vector<std::uint8_t> bled = readback;

        auto indexOf = [&](std::int32_t u, std::int32_t v) -> std::uint32_t
        {
            u = std::min(width, std::max(0, u));
            v = std::min(height, std::max(0, v));
            return 4 * (v * width + u);
        };
        auto isMiss = [](std::uint8_t* data)
        {
            return data[0] == 255 &&
                   data[1] == 0 &&
                   data[2] == 255;
        };

        for (std::uint32_t v = 0; v < width; v++)
        {
            for (std::uint32_t u = 0; u < height; u++)
            {
                std::uint32_t pixel = indexOf(u, v);
                // fix seams
                if (isMiss(&readback[pixel]))
                {
                    std::uint32_t pixelUp, pixelDown, pixelLeft, pixelRight;
                    pixelUp = indexOf(u, v + 1);
                    pixelDown = indexOf(u, v - 1);
                    pixelLeft = indexOf(u - 1, v);
                    pixelRight = indexOf(u + 1, v);
                    if (!isMiss(&readback[pixelUp]))
                    {
                        bled[pixel + 0] = readback[pixelUp + 0];
                        bled[pixel + 1] = readback[pixelUp + 1];
                        bled[pixel + 2] = readback[pixelUp + 2];
                    }
                    else if (!isMiss(&readback[pixelDown]))
                    {
                        bled[pixel + 0] = readback[pixelDown + 0];
                        bled[pixel + 1] = readback[pixelDown + 1];
                        bled[pixel + 2] = readback[pixelDown + 2];
                    }
                    else if (!isMiss(&readback[pixelLeft]))
                    {
                        bled[pixel + 0] = readback[pixelLeft + 0];
                        bled[pixel + 1] = readback[pixelLeft + 1];
                        bled[pixel + 2] = readback[pixelLeft + 2];
                    }
                    else if (!isMiss(&readback[pixelRight]))
                    {
                        bled[pixel + 0] = readback[pixelRight + 0];
                        bled[pixel + 1] = readback[pixelRight + 1];
                        bled[pixel + 2] = readback[pixelRight + 2];
                    }
                    else
                    {
                        bled[pixel + 0] = 127;
                        bled[pixel + 1] = 127;
                        bled[pixel + 2] = 127;
                    }
                }
            }
        }

        char outputFilename[256];
        std::snprintf(outputFilename, 256, "%s_%d_%smap.png", std::filesystem::path(filename).stem().c_str(), 0, false ? "normal" : "light");
        stbi_write_png(outputFilename, width, height, 4, bled.data(), 4 * width);
        std::cout << "saved output as " << outputFilename << std::endl;
    }
#endif

    auto end = std::chrono::steady_clock::now();
    auto secs = std::chrono::duration_cast<std::chrono::duration<float>>(end - start);
    std::cout << "Elapsed: " << secs.count() << "s\n";

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
