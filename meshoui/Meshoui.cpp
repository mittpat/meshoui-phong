#include "Meshoui.h"
#include "DeviceVk.h"
#include "SwapChainVk.h"

#include <cstring>
#include <experimental/filesystem>
#include <fstream>

#include <lodepng.h>
#include <nv_dds.h>

#define FrameCount 2
#define MESHOUI_PROGRAM_DESC_LAYOUT 0
#define MESHOUI_MATERIAL_DESC_LAYOUT 1

namespace std { namespace filesystem = experimental::filesystem; }

using namespace Meshoui;

static DeviceVk        g_Device;
static VkInstance      g_Instance = VK_NULL_HANDLE;
static VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;
static VkRenderPass    g_RenderPass = VK_NULL_HANDLE;
static void            (*g_CheckVkResultFn)(VkResult err) = NULL;
static SwapChainVk     g_SwapChain;
static MoPipeline      g_Pipeline;

void texture(ImageBufferVk & imageBuffer, const std::string & filename, MoFloat3 fallbackColor, VkCommandPool commandPool, VkCommandBuffer commandBuffer)
{
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    unsigned width = 0, height = 0;
    std::vector<uint8_t> data;

    auto ddsfilename = std::filesystem::path(filename).replace_extension(".dds");
    auto pngfilename = std::filesystem::path(filename).replace_extension(".png");
    if (std::filesystem::exists(ddsfilename))
    {
        nv_dds::CDDSImage image;
        image.load(ddsfilename.u8string(), false);
        width = image.get_width();
        height = image.get_height();
        data.resize(image.get_size());
        std::memcpy(data.data(), image, image.get_size());

        #define GL_COMPRESSED_RGB_S3TC_DXT1_EXT   0x83F0
        #define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT  0x83F1
        #define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT  0x83F2
        #define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT  0x83F3
        switch (image.get_format())
        {
        case 0x83F0: format = VK_FORMAT_BC1_RGB_UNORM_BLOCK; break;
        case 0x83F1: format = VK_FORMAT_BC1_RGBA_UNORM_BLOCK; break;
        case 0x83F2: format = VK_FORMAT_BC2_UNORM_BLOCK; break;
        case 0x83F3: format = VK_FORMAT_BC3_UNORM_BLOCK; break;
        default: abort();
        }
    }
    else if (std::filesystem::exists(pngfilename))
    {
        unsigned error = lodepng::decode(data, width, height, pngfilename.u8string());
        if (error != 0)
        {
            printf("TextureLoader::loadPNG: error '%d' : '%s'\n", error, lodepng_error_text(error));
            abort();
        }
    }
    else
    {
        // use fallback
        width = height = 1;
        data.resize(4);
        data[0] = fallbackColor.x * 0xFF;
        data[1] = fallbackColor.y * 0xFF;
        data[2] = fallbackColor.z * 0xFF;
        data[3] =             1.0 * 0xFF;
    }

    // begin
    VkResult err = vkResetCommandPool(g_Device.device, commandPool, 0);
    g_CheckVkResultFn(err);
    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(commandBuffer, &begin_info);
    g_CheckVkResultFn(err);

    // create buffer
    g_Device.createBuffer(imageBuffer, {width, height, 1}, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

    // upload
    DeviceBufferVk upload;
    g_Device.createBuffer(upload, data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    g_Device.uploadBuffer(upload, data.size(), data.data());
    g_Device.transferBuffer(upload, imageBuffer, {width, height, 1}, commandBuffer);

    // end
    VkSubmitInfo end_info = {};
    end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &commandBuffer;
    err = vkEndCommandBuffer(commandBuffer);
    g_CheckVkResultFn(err);
    err = vkQueueSubmit(g_Device.queue, 1, &end_info, VK_NULL_HANDLE);
    g_CheckVkResultFn(err);

    // wait
    err = vkDeviceWaitIdle(g_Device.device);
    g_CheckVkResultFn(err);

    g_Device.deleteBuffer(upload);
}

struct MoMesh_T
{
    DeviceBufferVk vertexBuffer;
    DeviceBufferVk indexBuffer;
    size_t indexBufferSize;
};

struct MoMaterial_T
{
    VkSampler ambientSampler;
    VkSampler diffuseSampler;
    VkSampler normalSampler;
    VkSampler specularSampler;
    VkSampler emissiveSampler;
    // this descriptor set uses only immutable samplers, one set per swapchain
    VkDescriptorSet descriptorSet;
    ImageBufferVk ambientImage;
    ImageBufferVk diffuseImage;
    ImageBufferVk normalImage;
    ImageBufferVk specularImage;
    ImageBufferVk emissiveImage;
};

struct MoPipeline_T
{
    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;
    // the buffers bound to this descriptor set may change frame to frame, one set per frame
    VkDescriptorSetLayout descriptorSetLayout[MESHOUI_MATERIAL_DESC_LAYOUT+1];
    VkDescriptorSet descriptorSet[FrameCount];
    DeviceBufferVk uniformBuffer[FrameCount];
};

bool moInit(MoInitInfo *info, VkRenderPass renderPass)
{
    g_Instance = info->instance;
    g_Device.physicalDevice = info->physicalDevice;
    g_Device.device = info->device;
    g_Device.queueFamily = info->queueFamily;
    g_Device.queue = info->queue;
    g_RenderPass = renderPass;
    g_PipelineCache = info->pipelineCache;
    g_Device.descriptorPool = info->descriptorPool;
    g_Device.allocator = info->pAllocator;
    g_CheckVkResultFn = info->pCheckVkResultFn;

    MoPipelineCreateInfo pipelineCreateInfo = {};

    std::ifstream vertexStream("meshoui/resources/shaders/Phong.vert.spv", std::ifstream::binary);
    const std::vector<char> vertexShader = std::vector<char>(std::istreambuf_iterator<char>(vertexStream), std::istreambuf_iterator<char>());
    pipelineCreateInfo.pVertexShader = vertexShader.data();
    pipelineCreateInfo.vertexShaderSize = vertexShader.size();

    std::ifstream fragmentStream("meshoui/resources/shaders/Phong.frag.spv", std::ifstream::binary);
    const std::vector<char> fragmentShader = std::vector<char>(std::istreambuf_iterator<char>(fragmentStream), std::istreambuf_iterator<char>());
    pipelineCreateInfo.pFragmentShader = fragmentShader.data();
    pipelineCreateInfo.fragmentShaderSize = fragmentShader.size();

    moCreatePipeline(&pipelineCreateInfo, &g_Pipeline);
}

void moCreatePipeline(const MoPipelineCreateInfo *pCreateInfo, MoPipeline *pPipeline)
{
    MoPipeline pipeline = *pPipeline = new MoPipeline_T();

}

void moDestroyPipeline(MoPipeline pipeline)
{

}

void moCreateMesh(const MoMeshCreateInfo *pCreateInfo, MoMesh *pMesh)
{
    MoMesh mesh = *pMesh = new MoMesh_T();
    mesh->indexBufferSize = pCreateInfo->indexCount;

    VkDeviceSize vertex_size = pCreateInfo->vertexCount * sizeof(MoVertex);
    VkDeviceSize index_size = pCreateInfo->indexCount * sizeof(uint32_t);
    g_Device.createBuffer(mesh->vertexBuffer, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    g_Device.createBuffer(mesh->indexBuffer, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    g_Device.uploadBuffer(mesh->vertexBuffer, vertex_size, pCreateInfo->pVertices);
    g_Device.uploadBuffer(mesh->indexBuffer, index_size, pCreateInfo->pIndices);
}

void moDestroyMesh(MoMesh mesh)
{
    vkQueueWaitIdle(g_Device.queue);
    g_Device.deleteBuffer(mesh->vertexBuffer);
    g_Device.deleteBuffer(mesh->indexBuffer);
    delete mesh;
}

void moCreateMaterial(const MoMaterialCreateInfo *pCreateInfo, MoMaterial *pMaterial)
{
    MoMaterial material = *pMaterial = new MoMaterial_T();

    texture(material->ambientImage, pCreateInfo->textureAmbient, pCreateInfo->colorAmbient, pCreateInfo->commandPool, pCreateInfo->commandBuffer);
    texture(material->diffuseImage, pCreateInfo->textureDiffuse, pCreateInfo->colorDiffuse, pCreateInfo->commandPool, pCreateInfo->commandBuffer);
    texture(material->normalImage, pCreateInfo->textureNormal, {0.f, 0.f, 0.f}, pCreateInfo->commandPool, pCreateInfo->commandBuffer);
    texture(material->emissiveImage, pCreateInfo->textureEmissive, pCreateInfo->colorEmissive, pCreateInfo->commandPool, pCreateInfo->commandBuffer);
    texture(material->specularImage, pCreateInfo->textureSpecular, pCreateInfo->colorSpecular, pCreateInfo->commandPool, pCreateInfo->commandBuffer);

    VkResult err;
    {
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;
        err = vkCreateSampler(g_Device.device, &info, g_Device.allocator, &material->ambientSampler);
        g_CheckVkResultFn(err);
        err = vkCreateSampler(g_Device.device, &info, g_Device.allocator, &material->diffuseSampler);
        g_CheckVkResultFn(err);
        err = vkCreateSampler(g_Device.device, &info, g_Device.allocator, &material->normalSampler);
        g_CheckVkResultFn(err);
        err = vkCreateSampler(g_Device.device, &info, g_Device.allocator, &material->specularSampler);
        g_CheckVkResultFn(err);
        err = vkCreateSampler(g_Device.device, &info, g_Device.allocator, &material->emissiveSampler);
        g_CheckVkResultFn(err);
    }

    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = g_Device.descriptorPool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &g_Pipeline->descriptorSetLayout[MESHOUI_MATERIAL_DESC_LAYOUT];
        err = vkAllocateDescriptorSets(g_Device.device, &alloc_info, &material->descriptorSet);
        g_CheckVkResultFn(err);
    }

    {
        VkDescriptorImageInfo desc_image[5] = {};
        desc_image[0].sampler = material->ambientSampler;
        desc_image[0].imageView = material->ambientImage.view;
        desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[1].sampler = material->diffuseSampler;
        desc_image[1].imageView = material->diffuseImage.view;
        desc_image[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[2].sampler = material->normalSampler;
        desc_image[2].imageView = material->normalImage.view;
        desc_image[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[3].sampler = material->specularSampler;
        desc_image[3].imageView = material->specularImage.view;
        desc_image[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        desc_image[4].sampler = material->emissiveSampler;
        desc_image[4].imageView = material->emissiveImage.view;
        desc_image[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet write_desc[5] = {};
        for (uint32_t i = 0; i < 5; ++i)
        {
            write_desc[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write_desc[i].dstSet = material->descriptorSet;
            write_desc[i].dstBinding = i;
            write_desc[i].descriptorCount = 1;
            write_desc[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write_desc[i].pImageInfo = &desc_image[i];
        }
        vkUpdateDescriptorSets(g_Device.device, 5, write_desc, 0, nullptr);
    }
}

void moDestroyMaterial(MoMaterial material)
{
    vkQueueWaitIdle(g_Device.queue);
    g_Device.deleteBuffer(material->ambientImage);
    g_Device.deleteBuffer(material->diffuseImage);
    g_Device.deleteBuffer(material->normalImage);
    g_Device.deleteBuffer(material->specularImage);
    g_Device.deleteBuffer(material->emissiveImage);

    vkDestroySampler(g_Device.device, material->ambientSampler, g_Device.allocator);
    vkDestroySampler(g_Device.device, material->diffuseSampler, g_Device.allocator);
    vkDestroySampler(g_Device.device, material->normalSampler, g_Device.allocator);
    vkDestroySampler(g_Device.device, material->specularSampler, g_Device.allocator);
    vkDestroySampler(g_Device.device, material->emissiveSampler, g_Device.allocator);
    delete material;
}
