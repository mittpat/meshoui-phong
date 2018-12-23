#pragma once

#include <vulkan/vulkan.h>

typedef struct MoMesh_T* MoMesh;
typedef struct MoMaterial_T* MoMaterial;
typedef struct MoPipeline_T* MoPipeline;

typedef struct MoImageBufferInfo
{
    VkImage       back;
    VkImageView   view;
    VkFramebuffer front;
} MoImageBufferInfo;

typedef struct MoCommandBufferInfo
{
    VkCommandPool   pool;
    VkCommandBuffer buffer;
    VkFence         fence;
    VkSemaphore     acquired;
    VkSemaphore     complete;
} MoCommandBufferInfo;

typedef struct MoInitInfo {
    VkInstance                   instance;
    VkPhysicalDevice             physicalDevice;
    VkDevice                     device;
    uint32_t                     queueFamily;
    VkQueue                      queue;
    VkPipelineCache              pipelineCache;
    VkDescriptorPool             descriptorPool;
    const MoImageBufferInfo*     pSwapChainImageBuffers;
    uint32_t                     swapChainImageBufferCount;
    const MoCommandBufferInfo*   pSwapChainCommandBuffers;
    uint32_t                     swapChainCommandBufferCount;
    VkSwapchainKHR               swapChainKHR;
    VkRenderPass                 renderPass;
    VkExtent2D                   extent;
    const VkAllocationCallbacks* pAllocator;
    void                         (*pCheckVkResultFn)(VkResult err);
} MoInitInfo;

typedef struct MoUInt3 {
    uint32_t x;
    uint32_t y;
    uint32_t z;
} MoUInt3;

typedef struct MoUInt3x3 {
    MoUInt3 x;
    MoUInt3 y;
    MoUInt3 z;
} MoUInt3x3;

typedef struct MoFloat2 {
    float x;
    float y;
} MoFloat2;

typedef struct MoFloat3 {
    float x;
    float y;
    float z;
} MoFloat3;

typedef struct MoFloat3x3 {
    MoFloat3 x;
    MoFloat3 y;
    MoFloat3 z;
} MoFloat3x3;

typedef struct MoFloat4 {
    float x;
    float y;
    float z;
    float w;
} MoFloat4;

typedef struct MoFloat4x4 {
    MoFloat4 x;
    MoFloat4 y;
    MoFloat4 z;
    MoFloat4 w;
} MoFloat4x4;

typedef struct MoVertex {
    MoFloat3 position;
    MoFloat2 texcoord;
    MoFloat3 normal;
} MoVertex;

typedef struct MoMeshCreateInfo {
    const uint32_t* pIndices;
    uint32_t        indexCount;
    const MoVertex* pVertices;
    uint32_t        vertexCount;
} MoMeshCreateInfo;

typedef struct MoMaterialCreateInfo {
    MoFloat3       colorAmbient;
    MoFloat3       colorDiffuse;
    MoFloat3       colorSpecular;
    MoFloat3       colorEmissive;
    // VK_FORMAT_R8G8B8A8_UNORM
    const uint8_t* pTextureAmbient;
    VkExtent2D     textureAmbientExtent;
    const uint8_t* pTextureDiffuse;
    VkExtent2D     textureDiffuseExtent;
    const uint8_t* pTextureNormal;
    VkExtent2D     textureNormalExtent;
    const uint8_t* pTextureSpecular;
    VkExtent2D     textureSpecularExtent;
    const uint8_t* pTextureEmissive;
    VkExtent2D     textureEmissiveExtent;
    VkDescriptorSetLayout setLayout;
} MoMaterialCreateInfo;

typedef struct MoPipelineCreateInfo {
    const uint32_t* pVertexShader;
    uint32_t        vertexShaderSize;
    const uint32_t* pFragmentShader;
    uint32_t        fragmentShaderSize;
} MoPipelineCreateInfo;

void moInit(MoInitInfo* info);
void moShutdown();
void moCreatePipeline(const MoPipelineCreateInfo *pCreateInfo, MoPipeline *pPipeline);
void moDestroyPipeline(MoPipeline pipeline);
void moCreateMesh(const MoMeshCreateInfo* pCreateInfo, MoMesh* pMesh);
void moDestroyMesh(MoMesh mesh);
void moCreateMaterial(const MoMaterialCreateInfo* pCreateInfo, MoMaterial* pMaterial);
void moDestroyMaterial(MoMaterial material);
void moNewFrame(uint32_t frameIndex);
void moSetPMV(const MoFloat4x4& projection, const MoFloat4x4& model, const MoFloat4x4& view);
void moSetLight(const MoFloat3& light, const MoFloat3& camera);
void moBindMesh(MoMesh mesh);
void moBindMaterial(MoMaterial material);
