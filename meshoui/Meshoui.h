#pragma once

#include <vulkan/vulkan.h>

typedef struct MoMesh_T* MoMesh;
typedef struct MoMaterial_T* MoMaterial;
typedef struct MoPipeline_T* MoPipeline;

typedef struct MoInitInfo {
    VkInstance                   instance;
    VkPhysicalDevice             physicalDevice;
    VkDevice                     device;
    uint32_t                     queueFamily;
    VkQueue                      queue;
    VkPipelineCache              pipelineCache;
    VkDescriptorPool             descriptorPool;
    const VkAllocationCallbacks* pAllocator;
    void                         (*pCheckVkResultFn)(VkResult err);
} MoInitInfo;

typedef struct MoFloat2 {
    float x;
    float y;
} MoFloat2;

typedef struct MoFloat3 {
    float x;
    float y;
    float z;
} MoFloat3;

typedef struct MoFloat4 {
    float x;
    float y;
    float z;
    float w;
} MoFloat4;

typedef struct MoVertex {
    MoFloat3 position;
    MoFloat2 texcoord;
    MoFloat3 normal;
    MoFloat3 tangent;
    MoFloat3 bitangent;
} MoVertex;

typedef struct MoMeshCreateInfo {
    const uint32_t* pIndices;
    uint32_t        indexCount;
    const MoVertex* pVertices;
    uint32_t        vertexCount;
} MoMeshCreateInfo;

typedef struct MoMaterialCreateInfo {
    MoFloat3        colorAmbient;
    MoFloat3        colorDiffuse;
    MoFloat3        colorSpecular;
    MoFloat3        colorEmissive;
    const char*     textureAmbient;
    const char*     textureDiffuse;
    const char*     textureNormal;
    const char*     textureSpecular;
    const char*     textureEmissive;
    VkCommandPool   commandPool;
    VkCommandBuffer commandBuffer;
} MoMaterialCreateInfo;

typedef struct MoPipelineCreateInfo {
    const char* pVertexShader;
    uint32_t    vertexShaderSize;
    const char* pFragmentShader;
    uint32_t    fragmentShaderSize;
} MoPipelineCreateInfo;

bool moInit(MoInitInfo* info, VkRenderPass renderPass);
void moCreatePipeline(const MoPipelineCreateInfo *pCreateInfo, MoPipeline *pPipeline);
void moDestroyPipeline(MoPipeline pipeline);
void moCreateMesh(const MoMeshCreateInfo* pCreateInfo, MoMesh* pMesh);
void moDestroyMesh(MoMesh mesh);
void moCreateMaterial(const MoMaterialCreateInfo* pCreateInfo, MoMaterial* pMaterial);
void moDestroyMaterial(MoMaterial material);
