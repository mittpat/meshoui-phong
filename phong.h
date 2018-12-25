#pragma once

#include <vulkan/vulkan.h>

typedef struct MoDevice_T* MoDevice;
typedef struct MoSwapChain_T* MoSwapChain;
typedef struct MoMesh_T* MoMesh;
typedef struct MoMaterial_T* MoMaterial;
typedef struct MoPipeline_T* MoPipeline;

typedef struct MoInstanceCreateInfo
{
    const char* const*           pExtensions;
    uint32_t                     extensionsCount;
    VkBool32                     debugReport;
    PFN_vkDebugReportCallbackEXT pDebugReportCallback;
    const VkAllocationCallbacks* pAllocator;
    void                       (*pCheckVkResultFn)(VkResult err);
} MoInstanceCreateInfo;

typedef struct MoDeviceCreateInfo
{
    VkInstance          instance;
    VkSurfaceKHR        surface;
    const VkFormat*     pRequestFormats;
    uint32_t            requestFormatsCount;
    VkColorSpaceKHR     requestColorSpace;
    VkSurfaceFormatKHR* pSurfaceFormat;
    void              (*pCheckVkResultFn)(VkResult err);
} MoDeviceCreateInfo;

typedef struct MoSwapChainCreateInfo
{
    MoDevice                     device;
    VkSurfaceKHR                 surface;
    VkSurfaceFormatKHR           surfaceFormat;
    VkExtent2D                   extent;
    VkBool32                     vsync;
    const VkAllocationCallbacks* pAllocator;
    void                       (*pCheckVkResultFn)(VkResult err);
} MoSwapChainCreateInfo;

typedef struct MoSwapChainRecreateInfo
{
    VkSurfaceKHR       surface;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D         extent;
    VkBool32           vsync;
} MoSwapChainRecreateInfo;

typedef struct MoSwapBuffer
{
    VkImage       back;
    VkImageView   view;
    VkFramebuffer front;
} MoSwapBuffer;

typedef struct MoCommandBuffer
{
    VkCommandPool   pool;
    VkCommandBuffer buffer;
    VkFence         fence;
    VkSemaphore     acquired;
    VkSemaphore     complete;
} MoCommandBuffer;

// version using only Vulkan API objects
// you must call moInit(MoInitInfo) before creating a mesh or material, typically when starting your application
// call moShutdown() when closing your application
typedef struct MoInitInfo {
    VkInstance                   instance;
    VkPhysicalDevice             physicalDevice;
    VkDevice                     device;
    uint32_t                     queueFamily;
    VkQueue                      queue;
    VkPipelineCache              pipelineCache;
    VkDescriptorPool             descriptorPool;
    const MoSwapBuffer*          pSwapChainSwapBuffers;
    uint32_t                     swapChainSwapBufferCount;
    const MoCommandBuffer*       pSwapChainCommandBuffers;
    uint32_t                     swapChainCommandBufferCount;
    VkSwapchainKHR               swapChainKHR;
    VkRenderPass                 renderPass;
    VkExtent2D                   extent;
    const VkAllocationCallbacks* pAllocator;
    void                         (*pCheckVkResultFn)(VkResult err);
} MoInitInfo;

// version using mixed Mo objects and Vulkan API objects
// you must call moInit(MoInitInfo2) before creating a mesh or material, typically when starting your application
// use moCreateDevice(MoDeviceCreateInfo) and moCreateSwapChain(MoSwapChainCreateInfo) to create device and swapchain
// call moShutdown() when closing your application
typedef struct MoInitInfo2 {
    VkInstance                   instance;
    MoDevice                     device;
    MoSwapChain                  swapChain;
    VkPipelineCache              pipelineCache;
    const VkAllocationCallbacks* pAllocator;
} MoInitInfo2;

// vector types, can be declared as your own so long as memory alignment is respected
#ifndef MOSKIPVECTYPES
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
#endif

// vertex type, can be declared as your own so long as memory alignment is respected
#ifndef MOSKIPVERTEXTYPE
typedef struct MoVertex {
    MoFloat3 position;
    MoFloat2 texcoord;
    MoFloat3 normal;
} MoVertex;
#endif

typedef struct MoMeshCreateInfo {
    const uint32_t* pIndices;
    uint32_t        indexCount;
    const MoVertex* pVertices;
    uint32_t        vertexCount;
    VkBool32        discardNormals;
    VkBool32        indicesCountFromOne;
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

// you can create a VkInstance using moCreateInstance(MoInstanceCreateInfo)
// but you do not have to; use moInit(MoInitInfo) to work off an existing instance
void moCreateInstance(MoInstanceCreateInfo* pCreateInfo, VkInstance* pInstance);

// free instance
void moDestroyInstance(VkInstance instance);

// you can create a VkDevice using moCreateDevice(MoDeviceCreateInfo)
// but you do not have to; use moInit(MoInitInfo) to work off an existing device
void moCreateDevice(MoDeviceCreateInfo* pCreateInfo, MoDevice* pDevice);

// free device
void moDestroyDevice(MoDevice device);

// you can create a VkSwapchain using moCreateSwapChain(MoSwapChainCreateInfo)
// but you do not have to; use moInit(MoInitInfo) to work off an existing swap chain
void moCreateSwapChain(MoSwapChainCreateInfo* pCreateInfo, MoSwapChain* pSwapChain);
void moRecreateSwapChain(MoSwapChainRecreateInfo* pCreateInfo, MoSwapChain swapChain);
void moBeginSwapChain(MoSwapChain swapChain, uint32_t *pFrameIndex, VkSemaphore *pImageAcquiredSemaphore);
VkResult moEndSwapChain(MoSwapChain swapChain, uint32_t *pFrameIndex, VkSemaphore *pImageAcquiredSemaphore);

// free swap chain, command and swap buffers
void moDestroySwapChain(MoDevice device, MoSwapChain pSwapChain);

// set global handles and create default phong pipeline
void moInit(MoInitInfo* pInfo);
void moInit(MoInitInfo2* pInfo);

// free default phong pipeline and clear global handles
void moShutdown();

// you can't really use another pipeline than the default phong pipeline in this header version, there's no reason to call this function
void moCreatePipeline(const MoPipelineCreateInfo *pCreateInfo, MoPipeline *pPipeline);

// idem
void moDestroyPipeline(MoPipeline pipeline);

// upload a new mesh to the GPU and return a handle
void moCreateMesh(const MoMeshCreateInfo* pCreateInfo, MoMesh* pMesh);

// free a mesh
void moDestroyMesh(MoMesh mesh);

// upload a new phong material to the GPU and return a handle
void moCreateMaterial(const MoMaterialCreateInfo* pCreateInfo, MoMaterial* pMaterial);

// free a material
void moDestroyMaterial(MoMaterial material);

// start a new frame by binding the default phong pipeline
void moNewFrame(uint32_t frameIndex);

// set view's projection and view matrices, and the mesh's model matrix (as a push constant)
void moSetPMV(const MoFloat4x4& projection, const MoFloat4x4& model, const MoFloat4x4& view);

// set the camera's position and light position (as a UBO)
void moSetLight(const MoFloat3& light, const MoFloat3& camera);

// bind a material
void moBindMaterial(MoMaterial material);

// draw a mesh
void moDrawMesh(MoMesh mesh);

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
