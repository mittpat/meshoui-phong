#include "Mesh.h"
#include "Program.h"
#include "Renderer.h"
#include "RendererPrivate.h"

#include <algorithm>
#include <experimental/filesystem>
#include <cstring>
#include <fstream>

#include <lodepng.h>
#include <nv_dds.h>

using namespace linalg;
using namespace linalg::aliases;
namespace std { namespace filesystem = experimental::filesystem; }
using namespace Meshoui;

namespace
{
    const float4x4 vkCorrMatrix = {{1.0f, 0.0f, 0.0f, 0.0f},
                                   {0.0f,-1.0f, 0.0f, 0.0f},
                                   {0.0f, 0.0f, 0.5f, 0.0f},
                                   {0.0f, 0.0f, 0.5f, 1.0f}};

    void check_vk_result(VkResult err)
    {
        if (err == 0) return;
        printf("VkResult %d\n", err);
        if (err < 0)
            abort();
    }

    template <typename T, size_t N> size_t countof(T (& arr)[N]) { return std::extent<T[N]>::value; }
    constexpr float degreesToRadians(float angle) { return angle * 3.14159265359f / 180.0f; }

    void texture(ImageBufferVk & imageBuffer, const std::string & filename, float4 fallbackColor, DeviceVk & device, SwapChainVk & swapChain, uint32_t frameIndex)
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
            data[3] = fallbackColor.w * 0xFF;
        }

        // Use any command queue
        VkCommandPool commandPool = swapChain.frames[frameIndex].pool;
        VkCommandBuffer commandBuffer = swapChain.frames[frameIndex].buffer;

        // begin
        VkResult err = vkResetCommandPool(device.device, commandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(commandBuffer, &begin_info);
        check_vk_result(err);

        // create buffer
        device.createBuffer(imageBuffer, {width, height, 1}, format, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT);

        // upload
        DeviceBufferVk upload;
        device.createBuffer(upload, data.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        device.uploadBuffer(upload, data.size(), data.data());
        device.transferBuffer(upload, imageBuffer, {width, height, 1}, commandBuffer);

        // end
        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &commandBuffer;
        err = vkEndCommandBuffer(commandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(device.queue, 1, &end_info, VK_NULL_HANDLE);
        check_vk_result(err);

        // wait
        err = vkDeviceWaitIdle(device.device);
        check_vk_result(err);

        device.deleteBuffer(upload);
    }
}

RendererPrivate::RendererPrivate(Renderer * r)
    : renderer(r)
    , window(nullptr)
    , instance()
    , device()
    , swapChain()
    , frameIndex(0)
    , pipelineCache(VK_NULL_HANDLE)
    , surface(VK_NULL_HANDLE)
    , surfaceFormat()
    , depthBuffer()
    , toFullscreen(false)
    , isFullscreen(false)
    , toVSync(true)
    , isVSync(true)
    , projectionMatrix(mul(vkCorrMatrix, linalg::perspective_matrix(degreesToRadians(100.f), 1920/1080.f, 0.1f, 1000.f)))
    , camera(nullptr)
{
    memset(&surfaceFormat, 0, sizeof(surfaceFormat));
}

void RendererPrivate::registerGraphics(Model *model)
{
    model->d = this;
    load(model->filename);
}

void RendererPrivate::registerGraphics(Mesh * mesh)
{
    // mesh
    {
        if (mesh->d != nullptr)
            abort();

        for (auto * other : renderer->meshes)
        {
            if (other != mesh && other->definitionId == mesh->definitionId)
            {
                mesh->d = other->d;
            }
        }
        if (mesh->d == nullptr)
        {
            const auto & meshFile = load(mesh->filename);
            auto definition = std::find_if(meshFile.definitions.begin(), meshFile.definitions.end(), [mesh](const MeshDefinition &definition)
            {
                return definition.definitionId == mesh->definitionId;
            });
            if (definition == meshFile.definitions.end())
            {
                abort();
            }

            MeshPrivate * meshPrivate = mesh->d = new MeshPrivate();
            meshPrivate->indexBufferSize = definition->indices.size();
            meshPrivate->definitionId = definition->definitionId;

            VkDeviceSize vertex_size = definition->vertices.size() * sizeof(Vertex);
            VkDeviceSize index_size = definition->indices.size() * sizeof(unsigned int);
            device.createBuffer(meshPrivate->vertexBuffer, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            device.createBuffer(meshPrivate->indexBuffer, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
            device.uploadBuffer(meshPrivate->vertexBuffer, vertex_size, definition->vertices.data());
            device.uploadBuffer(meshPrivate->indexBuffer, index_size, definition->indices.data());
        }
        mesh->d->referenceCount += 1;
    }

    //material
    {
        if (mesh->m != nullptr)
            abort();

        for (auto * other : renderer->meshes)
        {
            if (other != mesh && other->materialId == mesh->materialId)
            {
                mesh->m = other->m;
            }
        }
        if (mesh->m == nullptr)
        {
            const auto & meshFile = load(mesh->filename);
            auto material = std::find_if(meshFile.materials.begin(), meshFile.materials.end(), [mesh](const MeshMaterial &material)
            {
                return material.materialId == mesh->materialId;
            });
            if (material == meshFile.materials.end())
            {
                abort();
            }

            MaterialPrivate * materialPrivate = mesh->m = new MaterialPrivate();
            materialPrivate->materialId = material->materialId;
            texture(materialPrivate->ambientImage, material->textureAmbient, float4(material->ambient, 1.0f), device, swapChain, frameIndex);
            texture(materialPrivate->diffuseImage, material->textureDiffuse, float4(material->diffuse, 1.0f), device, swapChain, frameIndex);
            texture(materialPrivate->normalImage, material->textureNormal, linalg::zero, device, swapChain, frameIndex);
            texture(materialPrivate->emissiveImage, material->textureEmissive, float4(material->emissive, 1.0f), device, swapChain, frameIndex);
            texture(materialPrivate->specularImage, material->textureSpecular, float4(material->specular, 1.0f), device, swapChain, frameIndex);

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
                err = vkCreateSampler(device.device, &info, device.allocator, &materialPrivate->ambientSampler);
                check_vk_result(err);
                err = vkCreateSampler(device.device, &info, device.allocator, &materialPrivate->diffuseSampler);
                check_vk_result(err);
                err = vkCreateSampler(device.device, &info, device.allocator, &materialPrivate->normalSampler);
                check_vk_result(err);
                err = vkCreateSampler(device.device, &info, device.allocator, &materialPrivate->specularSampler);
                check_vk_result(err);
                err = vkCreateSampler(device.device, &info, device.allocator, &materialPrivate->emissiveSampler);
                check_vk_result(err);
            }

            {
                VkDescriptorSetAllocateInfo alloc_info = {};
                alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                alloc_info.descriptorPool = device.descriptorPool;
                alloc_info.descriptorSetCount = 1;
                alloc_info.pSetLayouts = &mesh->program->d->descriptorSetLayout[MESHOUI_MATERIAL_DESC_LAYOUT];
                err = vkAllocateDescriptorSets(device.device, &alloc_info, &materialPrivate->descriptorSet);
                check_vk_result(err);
            }

            {
                VkDescriptorImageInfo desc_image[5] = {};
                desc_image[0].sampler = materialPrivate->ambientSampler;
                desc_image[0].imageView = materialPrivate->ambientImage.view;
                desc_image[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                desc_image[1].sampler = materialPrivate->diffuseSampler;
                desc_image[1].imageView = materialPrivate->diffuseImage.view;
                desc_image[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                desc_image[2].sampler = materialPrivate->normalSampler;
                desc_image[2].imageView = materialPrivate->normalImage.view;
                desc_image[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                desc_image[3].sampler = materialPrivate->specularSampler;
                desc_image[3].imageView = materialPrivate->specularImage.view;
                desc_image[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                desc_image[4].sampler = materialPrivate->emissiveSampler;
                desc_image[4].imageView = materialPrivate->emissiveImage.view;
                desc_image[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkWriteDescriptorSet write_desc[5] = {};
                for (uint32_t i = 0; i < 5; ++i)
                {
                    write_desc[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                    write_desc[i].dstSet = materialPrivate->descriptorSet;
                    write_desc[i].dstBinding = i;
                    write_desc[i].descriptorCount = 1;
                    write_desc[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    write_desc[i].pImageInfo = &desc_image[i];
                }
                vkUpdateDescriptorSets(device.device, 5, write_desc, 0, nullptr);
            }
        }
        mesh->m->referenceCount += 1;
    }
}

void RendererPrivate::registerGraphics(Program * program)
{
    if (program->d != nullptr)
        abort();

    ProgramPrivate * programPrivate = program->d = new ProgramPrivate();

    VkResult err;
    VkShaderModule vert_module;
    VkShaderModule frag_module;

    // Create The Shader Modules:
    {
        VkShaderModuleCreateInfo vert_info = {};
        vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vert_info.codeSize = program->vertexShaderSource.size();
        vert_info.pCode = (uint32_t*)program->vertexShaderSource.data();
        err = vkCreateShaderModule(device.device, &vert_info, device.allocator, &vert_module);
        check_vk_result(err);
        VkShaderModuleCreateInfo frag_info = {};
        frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        frag_info.codeSize = program->fragmentShaderSource.size();
        frag_info.pCode = (uint32_t*)program->fragmentShaderSource.data();
        err = vkCreateShaderModule(device.device, &frag_info, device.allocator, &frag_module);
        check_vk_result(err);
    }

    {
        VkDescriptorSetLayoutBinding binding[2];
        binding[0].binding = 0;
        binding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding[0].descriptorCount = 1;
        binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
        binding[1].binding = 1;
        binding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding[1].descriptorCount = 1;
        binding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = countof(binding);
        info.pBindings = binding;
        err = vkCreateDescriptorSetLayout(device.device, &info, device.allocator, &programPrivate->descriptorSetLayout[MESHOUI_PROGRAM_DESC_LAYOUT]);
        check_vk_result(err);
    }

    {
        VkDescriptorSetLayoutBinding binding[5];
        for (uint32_t i = 0; i < 5; ++i)
        {
            binding[i].binding = i;
            binding[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding[i].descriptorCount = 1;
            binding[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            binding[i].pImmutableSamplers = VK_NULL_HANDLE;
        }
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = countof(binding);
        info.pBindings = binding;
        err = vkCreateDescriptorSetLayout(device.device, &info, device.allocator, &programPrivate->descriptorSetLayout[MESHOUI_MATERIAL_DESC_LAYOUT]);
        check_vk_result(err);
    }

    {
        VkDescriptorSetLayout descriptorSetLayout[FrameCount] = {};
        for (size_t i = 0; i < FrameCount; ++i)
            descriptorSetLayout[i] = programPrivate->descriptorSetLayout[MESHOUI_PROGRAM_DESC_LAYOUT];
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = device.descriptorPool;
        alloc_info.descriptorSetCount = FrameCount;
        alloc_info.pSetLayouts = descriptorSetLayout;
        err = vkAllocateDescriptorSets(device.device, &alloc_info, programPrivate->descriptorSet);
        check_vk_result(err);
    }

    for (size_t i = 0; i < FrameCount; ++i)
    {
        device.createBuffer(programPrivate->uniformBuffer[i], sizeof(Blocks::Uniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        VkDescriptorBufferInfo bufferInfo[1] = {};
        bufferInfo[0].buffer = programPrivate->uniformBuffer[i].buffer;
        bufferInfo[0].offset = 0;
        bufferInfo[0].range = sizeof(Blocks::Uniform);

        VkWriteDescriptorSet descriptorWrite[1] = {};
        descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite[0].dstSet = programPrivate->descriptorSet[i];
        descriptorWrite[0].dstBinding = 0;
        descriptorWrite[0].dstArrayElement = 0;
        descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite[0].descriptorCount = 1;
        descriptorWrite[0].pBufferInfo = &bufferInfo[0];

        vkUpdateDescriptorSets(device.device, 1, descriptorWrite, 0, nullptr);
    }

    {
        // model, view & projection
        std::vector<VkPushConstantRange> push_constants;
        push_constants.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Blocks::PushConstant)});
        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = countof(programPrivate->descriptorSetLayout);
        layout_info.pSetLayouts = programPrivate->descriptorSetLayout;
        layout_info.pushConstantRangeCount = push_constants.size();
        layout_info.pPushConstantRanges = push_constants.data();
        err = vkCreatePipelineLayout(device.device, &layout_info, device.allocator, &programPrivate->pipelineLayout);
        check_vk_result(err);
    }

    VkPipelineShaderStageCreateInfo stage[2] = {};
    stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage[0].module = vert_module;
    stage[0].pName = "main";
    stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage[1].module = frag_module;
    stage[1].pName = "main";

    VkVertexInputBindingDescription binding_desc[1] = {};
    binding_desc[0].stride = sizeof(Vertex);
    binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attribute_desc;
    attribute_desc.emplace_back(VkVertexInputAttributeDescription{uint32_t(attribute_desc.size()), binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, position)});
    attribute_desc.emplace_back(VkVertexInputAttributeDescription{uint32_t(attribute_desc.size()), binding_desc[0].binding, VK_FORMAT_R32G32_SFLOAT,    offsetof(struct Vertex, texcoord)});
    attribute_desc.emplace_back(VkVertexInputAttributeDescription{uint32_t(attribute_desc.size()), binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, normal)});
    attribute_desc.emplace_back(VkVertexInputAttributeDescription{uint32_t(attribute_desc.size()), binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, tangent)});
    attribute_desc.emplace_back(VkVertexInputAttributeDescription{uint32_t(attribute_desc.size()), binding_desc[0].binding, VK_FORMAT_R32G32B32_SFLOAT, offsetof(struct Vertex, bitangent)});

    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_info.vertexBindingDescriptionCount = 1;
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = attribute_desc.size();
    vertex_info.pVertexAttributeDescriptions = attribute_desc.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.cullMode = VK_CULL_MODE_BACK_BIT;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_info = {};
    ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_attachment[1] = {};
    color_attachment[0].blendEnable = VK_TRUE;
    color_attachment[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_attachment[0].alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_info.depthTestEnable = (program->features & Feature::DepthTest) ? VK_TRUE : VK_FALSE;
    depth_info.depthWriteEnable = (program->features & Feature::DepthWrite) ? VK_TRUE : VK_FALSE;
    depth_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_info.depthBoundsTestEnable = VK_FALSE;
    depth_info.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = countof(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.flags = 0;
    info.stageCount = 2;
    info.pStages = stage;
    info.pVertexInputState = &vertex_info;
    info.pInputAssemblyState = &inputAssembly;
    info.pViewportState = &viewport_info;
    info.pRasterizationState = &raster_info;
    info.pMultisampleState = &ms_info;
    info.pDepthStencilState = &depth_info;
    info.pColorBlendState = &blend_info;
    info.pDynamicState = &dynamic_state;
    info.layout = programPrivate->pipelineLayout;
    info.renderPass = swapChain.renderPass;
    err = vkCreateGraphicsPipelines(device.device, pipelineCache, 1, &info, device.allocator, &programPrivate->pipeline);
    check_vk_result(err);

    vkDestroyShaderModule(device.device, frag_module, nullptr);
    vkDestroyShaderModule(device.device, vert_module, nullptr);
}

void RendererPrivate::registerGraphics(Camera * cam)
{
    cam->d = this;
}

void RendererPrivate::registerGraphics(const MeshFile &)
{
    //
}

void RendererPrivate::unregisterGraphics(Model * model)
{
    model->d = nullptr;
}

void RendererPrivate::unregisterGraphics(Mesh * mesh)
{
    //mesh
    {
        if (mesh->d == nullptr)
            abort();

        MeshPrivate * meshPrivate = mesh->d;

        if (meshPrivate->referenceCount > 0)
            meshPrivate->referenceCount--;
        if (meshPrivate->referenceCount == 0)
        {
            vkQueueWaitIdle(device.queue);
            device.deleteBuffer(meshPrivate->vertexBuffer);
            device.deleteBuffer(meshPrivate->indexBuffer);
        }

        delete mesh->d;
        mesh->d = nullptr;
    }

    //material
    {
        if (mesh->m == nullptr)
            abort();

        MaterialPrivate * materialPrivate = mesh->m;

        if (materialPrivate->referenceCount > 0)
            materialPrivate->referenceCount--;
        if (materialPrivate->referenceCount == 0)
        {
            vkQueueWaitIdle(device.queue);
            device.deleteBuffer(materialPrivate->ambientImage);
            device.deleteBuffer(materialPrivate->diffuseImage);
            device.deleteBuffer(materialPrivate->normalImage);
            device.deleteBuffer(materialPrivate->specularImage);
            device.deleteBuffer(materialPrivate->emissiveImage);

            vkDestroySampler(device.device, materialPrivate->ambientSampler, device.allocator);
            vkDestroySampler(device.device, materialPrivate->diffuseSampler, device.allocator);
            vkDestroySampler(device.device, materialPrivate->normalSampler, device.allocator);
            vkDestroySampler(device.device, materialPrivate->specularSampler, device.allocator);
            vkDestroySampler(device.device, materialPrivate->emissiveSampler, device.allocator);
        }

        delete mesh->d;
        mesh->d = nullptr;
    }
}

void RendererPrivate::unregisterGraphics(Program * program)
{
    if (program->d == nullptr)
        abort();

    ProgramPrivate * programPrivate = program->d;

    vkQueueWaitIdle(device.queue);
    for (size_t i = 0; i < FrameCount; ++i)
        device.deleteBuffer(programPrivate->uniformBuffer[i]);

    vkDestroyDescriptorSetLayout(device.device, programPrivate->descriptorSetLayout[MESHOUI_PROGRAM_DESC_LAYOUT], device.allocator);
    vkDestroyDescriptorSetLayout(device.device, programPrivate->descriptorSetLayout[MESHOUI_MATERIAL_DESC_LAYOUT], device.allocator);
    vkDestroyPipelineLayout(device.device, programPrivate->pipelineLayout, device.allocator);
    vkDestroyPipeline(device.device, programPrivate->pipeline, device.allocator);
    programPrivate->descriptorSetLayout[MESHOUI_PROGRAM_DESC_LAYOUT] = VK_NULL_HANDLE;
    programPrivate->descriptorSetLayout[MESHOUI_MATERIAL_DESC_LAYOUT] = VK_NULL_HANDLE;
    programPrivate->pipelineLayout = VK_NULL_HANDLE;
    programPrivate->pipeline = VK_NULL_HANDLE;
    memset(&programPrivate->uniformBuffer, 0, sizeof(programPrivate->uniformBuffer));
    memset(&programPrivate->descriptorSet, 0, sizeof(programPrivate->descriptorSet));

    delete program->d;
    program->d = nullptr;
}

void RendererPrivate::unregisterGraphics(Camera *cam)
{
    unbindGraphics(cam);
    cam->d = nullptr;
}

void RendererPrivate::bindGraphics(Mesh * mesh)
{
    if (mesh->d == nullptr)
        abort();

    MeshPrivate * meshPrivate = mesh->d;

    VkDeviceSize offset = 0;
    auto & frame = swapChain.frames[frameIndex];
    vkCmdBindVertexBuffers(frame.buffer, 0, 1, &meshPrivate->vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(frame.buffer, meshPrivate->indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    MaterialPrivate * materialPrivate = mesh->m;

    vkCmdBindDescriptorSets(frame.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mesh->program->d->pipelineLayout, 1, 1, &materialPrivate->descriptorSet, 0, nullptr);
}

void RendererPrivate::bindGraphics(Program * program)
{
    if (program->d == nullptr)
        abort();

    ProgramPrivate * programPrivate = program->d;

    auto & frame = swapChain.frames[frameIndex];
    vkCmdBindPipeline(frame.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, programPrivate->pipeline);
    vkCmdBindDescriptorSets(frame.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, programPrivate->pipelineLayout, 0, 1, &programPrivate->descriptorSet[frameIndex], 0, nullptr);

    device.uploadBuffer(programPrivate->uniformBuffer[frameIndex], sizeof(Blocks::Uniform), &uniforms);
}

void RendererPrivate::bindGraphics(Camera *cam, bool asLight)
{
    if (asLight && std::find(lights.begin(), lights.end(), cam) == lights.end())
        lights.push_back(cam);
    else
        camera = cam;
}

void RendererPrivate::unbindGraphics(Mesh *)
{
    //
}

void RendererPrivate::unbindGraphics(Program *)
{
    //
}

void RendererPrivate::unbindGraphics(Camera *cam)
{
    if (camera == cam)
        camera = nullptr;
    if (std::find(lights.begin(), lights.end(), cam) != lights.end())
        lights.erase(std::remove(lights.begin(), lights.end(), cam));
}

void RendererPrivate::draw(Mesh *mesh)
{
    auto & frame = swapChain.frames[frameIndex];

    vkCmdPushConstants(frame.buffer, mesh->program->d->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Blocks::PushConstant), &pushConstants);
    vkCmdDrawIndexed(frame.buffer, mesh->d->indexBufferSize, 1, 0, 0, 0);
}

void RendererPrivate::fill(const std::string &filename, const std::vector<Mesh *> &meshes)
{
    const MeshFile & meshFile = load(filename);
    for (size_t i = 0; i < meshFile.instances.size(); ++i)
    {
        const MeshInstance & instance = meshFile.instances[i];
        Mesh * mesh = meshes[i];
        mesh->instanceId = instance.instanceId;
        mesh->definitionId = instance.definitionId;
        mesh->materialId = instance.materialId;
        mesh->filename = meshFile.filename;
        mesh->modelMatrix = instance.modelMatrix;
    }
}

const MeshFile& RendererPrivate::load(const std::string &filename)
{
    auto foundFile = std::find_if(meshFiles.begin(), meshFiles.end(), [filename](const MeshFile &meshFile)
    {
        return meshFile.filename == filename;
    });
    if (foundFile == meshFiles.end())
    {
        MeshFile meshFile;
        if (MeshLoader::load(filename, meshFile))
        {
            registerGraphics(meshFile);
            meshFiles.push_back(meshFile);
            return meshFiles.back();
        }
        else
        {
            static const MeshFile Invalid;
            return Invalid;
        }
    }
    else
    {
        return *foundFile;
    }
}
