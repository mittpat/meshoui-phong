#include "Mesh.h"
#include "Program.h"
#include "RendererPrivate.h"
#include "TextureLoader.h"

#include <loose.h>
#include <algorithm>
#include <experimental/filesystem>
#include <fstream>

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

    const ProgramRegistration & registrationFor(const ProgramRegistrations & programRegistrations, Program * program)
    {
        auto found = std::find_if(programRegistrations.begin(), programRegistrations.end(), [program](const std::pair<Program*, ProgramRegistration> & pair)
        {
            return pair.first == program;
        });
        if (found != programRegistrations.end())
        {
            return found->second;
        }
        static const ProgramRegistration Invalid;
        return Invalid;
    }

    const MeshRegistration & registrationFor(const MeshRegistrations & meshRegistrations, Mesh * mesh)
    {
        if (mesh != nullptr)
        {
            auto found = std::find_if(meshRegistrations.begin(), meshRegistrations.end(), [mesh](const MeshRegistration & meshRegistration)
            {
                return meshRegistration.definitionId == mesh->definitionId;
            });
            if (found != meshRegistrations.end())
            {
                return *found;
            }
        }
        static const MeshRegistration Invalid;
        return Invalid;
    }

    void texture(/*GLuint * buffer, */const std::string & filename, bool repeat)
    {
        if (TextureLoader::loadDDS(/*buffer, */std::filesystem::path(filename).replace_extension(".dds"), repeat))
            return;
        if (TextureLoader::loadPNG(/*buffer, */std::filesystem::path(filename).replace_extension(".png"), repeat))
            return;
    }
}

void RendererPrivate::unregisterProgram(ProgramRegistration & programRegistration)
{
    vkQueueWaitIdle(device.queue);
    for (size_t i = 0; i < FrameCount; ++i)
    {
        device.deleteBuffer(programRegistration.uniformBuffer[i]);
        device.deleteBuffer(programRegistration.materialBuffer[i]);
    }

    vkDestroyDescriptorSetLayout(device.device, programRegistration.descriptorSetLayout, device.allocator);
    vkDestroyPipelineLayout(device.device, programRegistration.pipelineLayout, device.allocator);
    vkDestroyPipeline(device.device, programRegistration.pipeline, device.allocator);
    programRegistration.descriptorSetLayout = VK_NULL_HANDLE;
    programRegistration.pipelineLayout = VK_NULL_HANDLE;
    programRegistration.pipeline = VK_NULL_HANDLE;
    memset(&programRegistration.materialBuffer, 0, sizeof(programRegistration.materialBuffer));
    memset(&programRegistration.uniformBuffer, 0, sizeof(programRegistration.uniformBuffer));
    memset(&programRegistration.descriptorSet, 0, sizeof(programRegistration.descriptorSet));
}

bool RendererPrivate::registerProgram(Program * program, ProgramRegistration & programRegistration)
{
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

//    if (!g_FontSampler)
//    {
//        VkSamplerCreateInfo info = {};
//        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
//        info.magFilter = VK_FILTER_LINEAR;
//        info.minFilter = VK_FILTER_LINEAR;
//        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
//        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//        info.minLod = -1000;
//        info.maxLod = 1000;
//        info.maxAnisotropy = 1.0f;
//        err = vkCreateSampler(g_Device, &info, g_Allocator, &g_FontSampler);
//        check_vk_result(err);
//    }

    {
        VkDescriptorSetLayoutBinding binding[2] = {};
        binding[0].binding = 0;
        binding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding[0].descriptorCount = 1;
        binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        binding[1].binding = 1;
        binding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        binding[1].descriptorCount = 1;
        binding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 2;
        info.pBindings = binding;
        err = vkCreateDescriptorSetLayout(device.device, &info, device.allocator, &programRegistration.descriptorSetLayout);
        check_vk_result(err);
    }

    {
        VkDescriptorSetLayout descriptorSetLayout[FrameCount] = {};
        for (size_t i = 0; i < FrameCount; ++i)
            descriptorSetLayout[i] = programRegistration.descriptorSetLayout;
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = device.descriptorPool;
        alloc_info.descriptorSetCount = FrameCount;
        alloc_info.pSetLayouts = descriptorSetLayout;
        err = vkAllocateDescriptorSets(device.device, &alloc_info, programRegistration.descriptorSet);
        check_vk_result(err);
    }

    for (size_t i = 0; i < FrameCount; ++i)
    {
        device.createBuffer(programRegistration.uniformBuffer[i], sizeof(Blocks::Uniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
        device.createBuffer(programRegistration.materialBuffer[i], sizeof(Blocks::PhongMaterial), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

        VkDescriptorBufferInfo bufferInfo[2] = {};
        bufferInfo[0].buffer = programRegistration.uniformBuffer[i].buffer;
        bufferInfo[0].offset = 0;
        bufferInfo[0].range = sizeof(Blocks::Uniform);
        bufferInfo[1].buffer = programRegistration.materialBuffer[i].buffer;
        bufferInfo[1].offset = 0;
        bufferInfo[1].range = sizeof(Blocks::PhongMaterial);

        VkWriteDescriptorSet descriptorWrite[2] = {};
        descriptorWrite[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite[0].dstSet = programRegistration.descriptorSet[i];
        descriptorWrite[0].dstBinding = 0;
        descriptorWrite[0].dstArrayElement = 0;
        descriptorWrite[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite[0].descriptorCount = 1;
        descriptorWrite[0].pBufferInfo = &bufferInfo[0];
        descriptorWrite[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite[1].dstSet = programRegistration.descriptorSet[i];
        descriptorWrite[1].dstBinding = 1;
        descriptorWrite[1].dstArrayElement = 0;
        descriptorWrite[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite[1].descriptorCount = 1;
        descriptorWrite[1].pBufferInfo = &bufferInfo[1];

        vkUpdateDescriptorSets(device.device, 2, descriptorWrite, 0, nullptr);
    }

    {
        // model, view & projection
        std::vector<VkPushConstantRange> push_constants;
        push_constants.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Blocks::PushConstant)});
        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &programRegistration.descriptorSetLayout;
        layout_info.pushConstantRangeCount = push_constants.size();
        layout_info.pPushConstantRanges = push_constants.data();
        err = vkCreatePipelineLayout(device.device, &layout_info, device.allocator, &programRegistration.pipelineLayout);
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
    inputAssembly.primitiveRestartEnable = VK_FALSE;

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

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = countof(dynamic_states);
    dynamic_state.pDynamicStates = dynamic_states;

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_info.depthTestEnable = (program->features & Feature::DepthTest) ? VK_TRUE : VK_FALSE;
    depth_info.depthWriteEnable = (program->features & Feature::DepthWrite) ? VK_TRUE : VK_FALSE;
    depth_info.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_info.depthBoundsTestEnable = VK_FALSE;
    depth_info.stencilTestEnable = VK_FALSE;

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
    info.layout = programRegistration.pipelineLayout;
    info.renderPass = renderPass;
    err = vkCreateGraphicsPipelines(device.device, pipelineCache, 1, &info, device.allocator, &programRegistration.pipeline);
    check_vk_result(err);

    vkDestroyShaderModule(device.device, frag_module, nullptr);
    vkDestroyShaderModule(device.device, vert_module, nullptr);

    return true;
}

void RendererPrivate::bindProgram(const ProgramRegistration & programRegistration)
{
    auto & frame = swapChain.frames[frameIndex];
    vkCmdBindPipeline(frame.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, programRegistration.pipeline);
    vkCmdBindDescriptorSets(frame.buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, programRegistration.pipelineLayout, 0, 1, &programRegistration.descriptorSet[frameIndex], 0, nullptr);

    Blocks::PhongMaterial def;
    device.uploadBuffer(programRegistration.uniformBuffer[frameIndex], sizeof(Blocks::Uniform), &uniforms);
    device.uploadBuffer(programRegistration.materialBuffer[frameIndex], sizeof(Blocks::PhongMaterial), &def);
}

void RendererPrivate::unbindProgram(const ProgramRegistration &)
{
    //
}

void RendererPrivate::unregisterMesh(const MeshRegistration & meshRegistration)
{
    vkQueueWaitIdle(device.queue);
    device.deleteBuffer(meshRegistration.vertexBuffer);
    device.deleteBuffer(meshRegistration.indexBuffer);
}

void RendererPrivate::registerMesh(const MeshDefinition & meshDefinition, MeshRegistration & meshRegistration)
{
    meshRegistration.indexBufferSize = meshDefinition.indices.size();
    meshRegistration.definitionId = meshDefinition.definitionId;

    VkDeviceSize vertex_size = meshDefinition.vertices.size() * sizeof(Vertex);
    VkDeviceSize index_size = meshDefinition.indices.size() * sizeof(unsigned int);
    device.createBuffer(meshRegistration.vertexBuffer, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    device.createBuffer(meshRegistration.indexBuffer, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    device.uploadBuffer(meshRegistration.vertexBuffer, vertex_size, meshDefinition.vertices.data());
    device.uploadBuffer(meshRegistration.indexBuffer, index_size, meshDefinition.indices.data());
}

void RendererPrivate::bindMesh(const MeshRegistration & meshRegistration, const ProgramRegistration &)
{
    VkDeviceSize offset = 0;
    auto & frame = swapChain.frames[frameIndex];
    vkCmdBindVertexBuffers(frame.buffer, 0, 1, &meshRegistration.vertexBuffer.buffer, &offset);
    vkCmdBindIndexBuffer(frame.buffer, meshRegistration.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
}

void RendererPrivate::unbindMesh(const MeshRegistration &, const ProgramRegistration &)
{
    //
}

RendererPrivate::RendererPrivate() 
    : window(nullptr)
    , instance()
    , device()
    , swapChain()
    , frameIndex(0)
    , pipelineCache(VK_NULL_HANDLE)
    , surface(VK_NULL_HANDLE)
    , surfaceFormat()
    , width(0)
    , height(0)
    , swapChainKHR(VK_NULL_HANDLE)
    , renderPass(VK_NULL_HANDLE)
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

void RendererPrivate::destroySwapChainAndFramebuffer()
{
    vkQueueWaitIdle(device.queue);
    device.deleteBuffer(depthBuffer);
    for (auto & image : swapChain.images)
    {
        vkDestroyImageView(device.device, image.view, device.allocator);
        vkDestroyFramebuffer(device.device, image.front, device.allocator);
    }
    vkDestroyRenderPass(device.device, renderPass, device.allocator);
    vkDestroySwapchainKHR(device.device, swapChainKHR, device.allocator);
    vkDestroySurfaceKHR(instance.instance, surface, device.allocator);
}

void RendererPrivate::createSwapChainAndFramebuffer(int w, int h, bool vsync)
{
    VkResult err;
    VkSwapchainKHR old_swapchain = swapChainKHR;
    err = vkDeviceWaitIdle(device.device);
    check_vk_result(err);

    for (auto & image : swapChain.images)
    {
        vkDestroyImageView(device.device, image.view, device.allocator);
        vkDestroyFramebuffer(device.device, image.front, device.allocator);
    }
    swapChain.images.resize(0);
    if (renderPass)
    {
        vkDestroyRenderPass(device.device, renderPass, device.allocator);
    }

    {
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = surface;
        info.minImageCount = FrameCount;
        info.imageFormat = surfaceFormat.format;
        info.imageColorSpace = surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
        info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = vsync ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = old_swapchain;
        VkSurfaceCapabilitiesKHR cap;
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.physicalDevice, surface, &cap);
        check_vk_result(err);
        if (info.minImageCount < cap.minImageCount)
            info.minImageCount = cap.minImageCount;
        else if (cap.maxImageCount != 0 && info.minImageCount > cap.maxImageCount)
            info.minImageCount = cap.maxImageCount;

        if (cap.currentExtent.width == 0xffffffff)
        {
            info.imageExtent.width = width = w;
            info.imageExtent.height = height = h;
        }
        else
        {
            info.imageExtent.width = width = cap.currentExtent.width;
            info.imageExtent.height = height = cap.currentExtent.height;
        }
        err = vkCreateSwapchainKHR(device.device, &info, device.allocator, &swapChainKHR);
        check_vk_result(err);
        uint32_t backBufferCount = 0;
        err = vkGetSwapchainImagesKHR(device.device, swapChainKHR, &backBufferCount, NULL);
        check_vk_result(err);
        std::vector<VkImage> backBuffer(backBufferCount);
        err = vkGetSwapchainImagesKHR(device.device, swapChainKHR, &backBufferCount, backBuffer.data());
        check_vk_result(err);

        swapChain.images.resize(backBufferCount);
        for (size_t i = 0; i < swapChain.images.size(); ++i)
        {
            swapChain.images[i].back = backBuffer[i];
        }
    }
    if (old_swapchain)
    {
        vkDestroySwapchainKHR(device.device, old_swapchain, device.allocator);
    }

    {
        VkAttachmentDescription attachment[2] = {};
        attachment[0].format = surfaceFormat.format;
        attachment[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment[1].format = VK_FORMAT_D16_UNORM;
        attachment[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachment[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;        
        VkAttachmentReference depth_attachment = {};
        depth_attachment.attachment = 1;
        depth_attachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        subpass.pDepthStencilAttachment = &depth_attachment;

        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        err = vkCreateRenderPass(device.device, &info, device.allocator, &renderPass);
        check_vk_result(err);
    }
    {
        VkImageViewCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = surfaceFormat.format;
        info.components.r = VK_COMPONENT_SWIZZLE_R;
        info.components.g = VK_COMPONENT_SWIZZLE_G;
        info.components.b = VK_COMPONENT_SWIZZLE_B;
        info.components.a = VK_COMPONENT_SWIZZLE_A;
        VkImageSubresourceRange image_range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        info.subresourceRange = image_range;
        for (size_t i = 0; i < swapChain.images.size(); ++i)
        {
            info.image = swapChain.images[i].back;
            err = vkCreateImageView(device.device, &info, device.allocator, &swapChain.images[i].view);
            check_vk_result(err);
        }
    }

    // depth buffer
    device.createBuffer(depthBuffer, {width, height, 1}, VK_FORMAT_D16_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_ASPECT_DEPTH_BIT);

    {
        VkImageView attachment[2] = {0, depthBuffer.view};
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderPass;
        info.attachmentCount = 2;
        info.pAttachments = attachment;
        info.width = width;
        info.height = height;
        info.layers = 1;
        for (size_t i = 0; i < swapChain.images.size(); ++i)
        {
            attachment[0] = swapChain.images[i].view;
            err = vkCreateFramebuffer(device.device, &info, device.allocator, &swapChain.images[i].front);
            check_vk_result(err);
        }
    }
}

void RendererPrivate::registerGraphics(Model *model)
{
    model->d = this;
    load(model->filename);
}

void RendererPrivate::registerGraphics(Mesh * mesh)
{
    const MeshRegistration * meshRegistration = &registrationFor(meshRegistrations, mesh);
    const_cast<MeshRegistration *>(meshRegistration)->referenceCount += 1;
}

void RendererPrivate::registerGraphics(Program * program)
{
    ProgramRegistration programRegistration;
    registerProgram(program, programRegistration);
    programRegistrations.push_back(std::make_pair(program, programRegistration));
}

void RendererPrivate::registerGraphics(Camera * cam)
{
    cam->d = this;
}

void RendererPrivate::registerGraphics(const MeshFile &meshFile)
{
    for (const auto & definition : meshFile.definitions)
    {
        printf("%s\n", definition.definitionId.str.c_str());

        MeshRegistration meshRegistration;
        registerMesh(definition, meshRegistration);
        meshRegistrations.push_back(meshRegistration);
    }
    for (const auto & material : meshFile.materials)
    {
        for (auto value : material.values)
        {
            if (value.texture)
            {
                TextureRegistration textureRegistration = TextureRegistration(*value.texture);
                texture(/*&textureRegistration.buffer, */sibling(*value.texture, meshFile.filename), material.repeatTexcoords);
                textureRegistrations.push_back(textureRegistration);
            }
        }
    }
}

void RendererPrivate::unregisterGraphics(Model *model)
{
    model->d = nullptr;
}

void RendererPrivate::unregisterGraphics(Mesh * mesh)
{
    auto found = std::find_if(meshRegistrations.begin(), meshRegistrations.end(), [mesh](const MeshRegistration & meshRegistration)
    {
        return meshRegistration.definitionId == mesh->definitionId;
    });
    if (found != meshRegistrations.end())
    {
        MeshRegistration & meshRegistration = *found;
        if (meshRegistration.referenceCount > 0)
            meshRegistration.referenceCount--;
        if (meshRegistration.referenceCount == 0)
        {
            unregisterMesh(meshRegistration);
            meshRegistrations.erase(found);
        }
    }
}

void RendererPrivate::unregisterGraphics(Program * program)
{
    auto found = std::find_if(programRegistrations.begin(), programRegistrations.end(), [program](const std::pair<Program*, ProgramRegistration> & pair)
    {
        return pair.first == program;
    });
    if (found != programRegistrations.end())
    {
        unregisterProgram(found->second);
        programRegistrations.erase(found);
    }
}

void RendererPrivate::unregisterGraphics(Camera *cam)
{
    unbindGraphics(cam);
    cam->d = nullptr;
}

void RendererPrivate::bindGraphics(Mesh * mesh)
{
    bindMesh(registrationFor(meshRegistrations, mesh), registrationFor(programRegistrations, mesh->program));
}

void RendererPrivate::bindGraphics(Program * program)
{
    bindProgram(registrationFor(programRegistrations, program));
}

void RendererPrivate::bindGraphics(Camera *cam, bool asLight)
{
    if (asLight && std::find(lights.begin(), lights.end(), cam) == lights.end())
        lights.push_back(cam);
    else
        camera = cam;
}

void RendererPrivate::unbindGraphics(Mesh * mesh)
{
    unbindMesh(registrationFor(meshRegistrations, mesh), registrationFor(programRegistrations, mesh->program));
}

void RendererPrivate::unbindGraphics(Program * program)
{
    unbindProgram(registrationFor(programRegistrations, program));
}

void RendererPrivate::unbindGraphics(Camera *cam)
{
    if (camera == cam)
        camera = nullptr;
    if (std::find(lights.begin(), lights.end(), cam) != lights.end())
        lights.erase(std::remove(lights.begin(), lights.end(), cam));
}

void RendererPrivate::draw(Program *program, Mesh *mesh)
{
    const ProgramRegistration & programRegistration = registrationFor(programRegistrations, program);
    const MeshRegistration & meshRegistration = registrationFor(meshRegistrations, mesh);

    auto & frame = swapChain.frames[frameIndex];

    vkCmdPushConstants(frame.buffer, programRegistration.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Blocks::PushConstant), &pushConstants);
    vkCmdDrawIndexed(frame.buffer, meshRegistration.indexBufferSize, 1, 0, 0, 0);
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
        mesh->filename = meshFile.filename;
        mesh->scale = instance.scale;
        mesh->position = instance.position;
        mesh->orientation = instance.orientation;
        auto definition = std::find_if(meshFile.definitions.begin(), meshFile.definitions.end(), [instance](const auto & definition){ return definition.definitionId == instance.definitionId; });
        if (definition->doubleSided)
        {
            mesh->renderFlags &= ~Render::BackFaceCulling;
        }
        auto material = std::find_if(meshFile.materials.begin(), meshFile.materials.end(), [instance](const MeshMaterial & material) { return material.name == instance.materialId; });
        for (auto value : material->values)
        {
            //
        }
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
