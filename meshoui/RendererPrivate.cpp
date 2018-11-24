#include "Mesh.h"
#include "Program.h"
#include "RendererPrivate.h"
#include "TextureLoader.h"
#include "Uniform.h"

#include <imgui.h>
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

    void texture(GLuint * buffer, const std::string & filename, bool repeat)
    {
        if (TextureLoader::loadDDS(buffer, std::filesystem::path(filename).replace_extension(".dds"), repeat))
            return;
        if (TextureLoader::loadPNG(buffer, std::filesystem::path(filename).replace_extension(".png"), repeat))
            return;
    }

    void setUniform(const TextureRegistrations &, Mesh *, IUniform *, const ProgramUniform &)
    {
        //
    }
}

void RendererPrivate::unregisterProgram(ProgramRegistration & programRegistration)
{
    vkDestroyDescriptorSetLayout(device, programRegistration.descriptorSetLayout, allocator);
    vkDestroyPipelineLayout(device, programRegistration.pipelineLayout, allocator);
    vkDestroyPipeline(device, programRegistration.pipeline, allocator);
    programRegistration.descriptorSetLayout = VK_NULL_HANDLE;
    programRegistration.pipelineLayout = VK_NULL_HANDLE;
    programRegistration.pipeline = VK_NULL_HANDLE;
}

bool RendererPrivate::registerProgram(Program * program, ProgramRegistration & programRegistration)
{
    glslLangOutputParse(programRegistration.pipelineReflectionInfo.vertexShaderStage, program->vertexShaderReflection.data());
    glslLangOutputParse(programRegistration.pipelineReflectionInfo.fragmentShaderStage, program->fragmentShaderReflection.data());

    VkResult err;
    VkShaderModule vert_module;
    VkShaderModule frag_module;

    // Create The Shader Modules:
    {
        VkShaderModuleCreateInfo vert_info = {};
        vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vert_info.codeSize = program->vertexShaderSource.size();
        vert_info.pCode = (uint32_t*)program->vertexShaderSource.data();
        err = vkCreateShaderModule(device, &vert_info, allocator, &vert_module);
        check_vk_result(err);
        VkShaderModuleCreateInfo frag_info = {};
        frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        frag_info.codeSize = program->fragmentShaderSource.size();
        frag_info.pCode = (uint32_t*)program->fragmentShaderSource.data();
        err = vkCreateShaderModule(device, &frag_info, allocator, &frag_module);
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
        //VkSampler sampler[1] = {g_FontSampler};
        VkDescriptorSetLayoutBinding binding[1] = {};
        binding[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding[0].descriptorCount = 1;
        binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        //binding[0].pImmutableSamplers = sampler;
        VkDescriptorSetLayoutCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings = binding;
        err = vkCreateDescriptorSetLayout(device, &info, allocator, &programRegistration.descriptorSetLayout);
        check_vk_result(err);
    }

    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = descriptorPool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &programRegistration.descriptorSetLayout;
        err = vkAllocateDescriptorSets(device, &alloc_info, &programRegistration.descriptorSet);
        check_vk_result(err);
    }

    {
        // model, view & projection
        std::vector<VkPushConstantRange> push_constants;
        for (const auto & uniformBlock : programRegistration.pipelineReflectionInfo.vertexShaderStage.uniformBlockReflection)
            push_constants.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_VERTEX_BIT, 0, uniformBlock.size});
        for (const auto & uniformBlock : programRegistration.pipelineReflectionInfo.fragmentShaderStage.uniformBlockReflection)
            push_constants.emplace_back(VkPushConstantRange{VK_SHADER_STAGE_FRAGMENT_BIT, 0, uniformBlock.size});
        VkDescriptorSetLayout set_layout[1] = { programRegistration.descriptorSetLayout };
        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = set_layout;
        layout_info.pushConstantRangeCount = push_constants.size();
        layout_info.pPushConstantRanges = push_constants.data();
        err = vkCreatePipelineLayout(device, &layout_info, allocator, &programRegistration.pipelineLayout);
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
    attribute_desc.reserve(programRegistration.pipelineReflectionInfo.vertexShaderStage.vertexAttributeReflection.size());
    for (const auto & vertexAttribute : programRegistration.pipelineReflectionInfo.vertexShaderStage.vertexAttributeReflection)
    {
        VkFormat format = VK_FORMAT_R32G32B32_SFLOAT;
        switch (vertexAttribute.type)
        {
        case GL_FLOAT_VEC2_ARB: format = VK_FORMAT_R32G32_SFLOAT;       break;
        case GL_FLOAT_VEC3_ARB: format = VK_FORMAT_R32G32B32_SFLOAT;    break;
        case GL_FLOAT_VEC4_ARB: format = VK_FORMAT_R32G32B32A32_SFLOAT; break;
        default:                format = VK_FORMAT_R32_SFLOAT;          break;
        }
        attribute_desc.emplace_back(VkVertexInputAttributeDescription{vertexAttribute.index, binding_desc[0].binding, format, Vertex::describe(vertexAttribute.name).offset});
    }

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
    raster_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = (uint32_t)IM_ARRAYSIZE(dynamic_states);
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
    info.layout = programRegistration.pipelineLayout;
    info.renderPass = renderPass;
    err = vkCreateGraphicsPipelines(device, pipelineCache, 1, &info, allocator, &programRegistration.pipeline);
    check_vk_result(err);

    vkDestroyShaderModule(device, frag_module, nullptr);
    vkDestroyShaderModule(device, vert_module, nullptr);

    return true;
}

void RendererPrivate::bindProgram(const ProgramRegistration & programRegistration)
{
    //
}

void RendererPrivate::unbindProgram(const ProgramRegistration & programRegistration)
{
    //
}

void RendererPrivate::unregisterMesh(const MeshRegistration & meshRegistration)
{
    //
}

void RendererPrivate::registerMesh(const MeshDefinition & meshDefinition, MeshRegistration & meshRegistration)
{
    //

    meshRegistration.indices = meshDefinition.indices;
    meshRegistration.vertices = meshDefinition.vertices;

    meshRegistration.indexBufferSize = meshDefinition.indices.size();
    meshRegistration.vertexBufferSize = meshDefinition.vertices.size();
    meshRegistration.definitionId = meshDefinition.definitionId;
}

void RendererPrivate::bindMesh(const MeshRegistration & meshRegistration, const ProgramRegistration & programRegistration)
{
    //

    //size_t offset = 0;
    //for (const Attribute & attributeDef : Vertex::Attributes)
    //{
    //    const auto & vertexAttributeReflection = programRegistration.pipelineReflectionInfo.vertexShaderStage.vertexAttributeReflection;
    //    auto found = std::find_if(vertexAttributeReflection.begin(), vertexAttributeReflection.end(), [attributeDef](const ProgramAttribute & attribute)
    //    {
    //        return attribute.name == attributeDef.name;
    //    });
    //    if (found != vertexAttributeReflection.end())
    //    {
    //        //
    //    }
    //    offset += attributeDef.size * sizeof(GLfloat);
    //}
}

void RendererPrivate::unbindMesh(const MeshRegistration &, const ProgramRegistration & programRegistration)
{
    //for (const Attribute & attributeDef : Vertex::Attributes)
    //{
    //    const auto & vertexAttributeReflection = programRegistration.pipelineReflectionInfo.vertexShaderStage.vertexAttributeReflection;
    //    auto found = std::find_if(vertexAttributeReflection.begin(), vertexAttributeReflection.end(), [attributeDef](const ProgramAttribute & attribute)
    //    {
    //        return attribute.name == attributeDef.name;
    //    });
    //    if (found != vertexAttributeReflection.end())
    //    {
    //        //
    //    }
    //}

    //
}

RendererPrivate::RendererPrivate() 
    : window(nullptr)
    , allocator(VK_NULL_HANDLE)
    , instance(VK_NULL_HANDLE)
    , physicalDevice(VK_NULL_HANDLE)
    , device(VK_NULL_HANDLE)
    , queueFamily(-1)
    , queue(VK_NULL_HANDLE)
    , pipelineCache(VK_NULL_HANDLE)
    , descriptorPool(VK_NULL_HANDLE)
    , surface(VK_NULL_HANDLE)
    , surfaceFormat()
    , presentMode(VK_PRESENT_MODE_MAX_ENUM_KHR)

    , width(0)
    , height(0)
    , swapchain(VK_NULL_HANDLE)
    , renderPass(VK_NULL_HANDLE)
    , backBufferCount(0)
    , backBuffer()
    , backBufferView()
    , framebuffer()
    , frames()
    , frameIndex(0)

    , toFullscreen(false)
    , fullscreen(false)
    , projectionMatrix(linalg::perspective_matrix(degreesToRadians(100.f), 1920/1080.f, 0.1f, 1000.f))
    , camera(nullptr)
{
    memset(&surfaceFormat, 0, sizeof(surfaceFormat));
    memset(&backBuffer, 0, sizeof(backBuffer));
    memset(&backBufferView, 0, sizeof(backBufferView));
    memset(&framebuffer, 0, sizeof(framebuffer));
    memset(&frames, 0, sizeof(frames));
}

void RendererPrivate::selectSurfaceFormat(const VkFormat* request_formats, int request_formats_count, VkColorSpaceKHR request_color_space)
{
    surfaceFormat.format = VK_FORMAT_UNDEFINED;

    uint32_t avail_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &avail_count, VK_NULL_HANDLE);
    ImVector<VkSurfaceFormatKHR> avail_format;
    avail_format.resize((int)avail_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &avail_count, avail_format.Data);

    if (avail_count == 1)
    {
        if (avail_format[0].format == VK_FORMAT_UNDEFINED)
        {
            surfaceFormat.format = request_formats[0];
            surfaceFormat.colorSpace = request_color_space;
        }
        else
        {
            surfaceFormat = avail_format[0];
        }
    }
    else
    {
        surfaceFormat = avail_format[0];
        for (int request_i = 0; request_i < request_formats_count; request_i++)
        {
            for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
            {
                if (avail_format[avail_i].format == request_formats[request_i] && avail_format[avail_i].colorSpace == request_color_space)
                {
                    surfaceFormat = avail_format[avail_i];
                }
            }
        }
    }
}

void RendererPrivate::selectPresentMode(const VkPresentModeKHR* request_modes, int request_modes_count)
{
    uint32_t avail_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &avail_count, VK_NULL_HANDLE);
    ImVector<VkPresentModeKHR> avail_modes;
    avail_modes.resize((int)avail_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &avail_count, avail_modes.Data);

    presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (int request_i = 0; request_i < request_modes_count; request_i++)
    {
        for (uint32_t avail_i = 0; avail_i < avail_count; avail_i++)
        {
            if (request_modes[request_i] == avail_modes[avail_i])
            {
                presentMode = request_modes[request_i];
            }
        }
    }
}

void RendererPrivate::destroyGraphicsSubsystem()
{
    vkDestroyDescriptorPool(device, descriptorPool, allocator);
    vkDestroyDevice(device, allocator);
    vkDestroyInstance(instance, allocator);
}

void RendererPrivate::createGraphicsSubsystem(const char* const* extensions, uint32_t extensions_count)
{
    VkResult err;

    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.enabledExtensionCount = extensions_count;
        create_info.ppEnabledExtensionNames = extensions;
        err = vkCreateInstance(&create_info, allocator, &instance);
        check_vk_result(err);
    }

    {
        uint32_t count;
        err = vkEnumeratePhysicalDevices(instance, &count, VK_NULL_HANDLE);
        check_vk_result(err);
        std::vector<VkPhysicalDevice> gpus(count);
        err = vkEnumeratePhysicalDevices(instance, &count, gpus.data());
        check_vk_result(err);
        physicalDevice = gpus[0];
    }

    {
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, VK_NULL_HANDLE);
        std::vector<VkQueueFamilyProperties> queues(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, queues.data());
        for (uint32_t i = 0; i < count; i++)
        {
            if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                queueFamily = i;
                break;
            }
        }
    }

    {
        uint32_t device_extensions_count = 1;
        const char* device_extensions[] = { "VK_KHR_swapchain" };
        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = queueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = IM_ARRAYSIZE(queue_info);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = device_extensions_count;
        create_info.ppEnabledExtensionNames = device_extensions;
        err = vkCreateDevice(physicalDevice, &create_info, allocator, &device);
        check_vk_result(err);
        vkGetDeviceQueue(device, queueFamily, 0, &queue);
    }

    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        pool_info.poolSizeCount = IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(device, &pool_info, allocator, &descriptorPool);
        check_vk_result(err);
    }
}

void RendererPrivate::destroyCommandBuffers()
{
    vkQueueWaitIdle(queue);
    for (int i = 0; i < 2; i++)
    {
        vkDestroyFence(device, frames[i].fence, allocator);
        vkFreeCommandBuffers(device, frames[i].commandPool, 1, &frames[i].commandBuffer);
        vkDestroyCommandPool(device, frames[i].commandPool, allocator);
        vkDestroySemaphore(device, frames[i].imageAcquiredSemaphore, allocator);
        vkDestroySemaphore(device, frames[i].renderCompleteSemaphore, allocator);
    }
}

void RendererPrivate::createCommandBuffers()
{
    VkResult err;
    for (int i = 0; i < 2; i++)
    {
        FrameData* fd = &frames[i];
        {
            VkCommandPoolCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            info.queueFamilyIndex = queueFamily;
            err = vkCreateCommandPool(device, &info, allocator, &fd->commandPool);
            check_vk_result(err);
        }
        {
            VkCommandBufferAllocateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            info.commandPool = fd->commandPool;
            info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            info.commandBufferCount = 1;
            err = vkAllocateCommandBuffers(device, &info, &fd->commandBuffer);
            check_vk_result(err);
        }
        {
            VkFenceCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            err = vkCreateFence(device, &info, allocator, &fd->fence);
            check_vk_result(err);
        }
        {
            VkSemaphoreCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            err = vkCreateSemaphore(device, &info, allocator, &fd->imageAcquiredSemaphore);
            check_vk_result(err);
            err = vkCreateSemaphore(device, &info, allocator, &fd->renderCompleteSemaphore);
            check_vk_result(err);
        }
    }
}

void RendererPrivate::destroySwapChainAndFramebuffer()
{
    vkQueueWaitIdle(queue);
    for (uint32_t i = 0; i < backBufferCount; i++)
    {
        vkDestroyImageView(device, backBufferView[i], allocator);
        vkDestroyFramebuffer(device, framebuffer[i], allocator);
    }
    vkDestroyRenderPass(device, renderPass, allocator);
    vkDestroySwapchainKHR(device, swapchain, allocator);
    vkDestroySurfaceKHR(instance, surface, allocator);
}

void RendererPrivate::createSwapChainAndFramebuffer(int w, int h)
{
    uint32_t min_image_count = 2;

    VkResult err;
    VkSwapchainKHR old_swapchain = swapchain;
    err = vkDeviceWaitIdle(device);
    check_vk_result(err);

    for (uint32_t i = 0; i < backBufferCount; i++)
    {
        if (backBufferView[i])
            vkDestroyImageView(device, backBufferView[i], allocator);
        if (framebuffer[i])
            vkDestroyFramebuffer(device, framebuffer[i], allocator);
    }
    backBufferCount = 0;
    if (renderPass)
    {
        vkDestroyRenderPass(device, renderPass, allocator);
    }

    if (min_image_count == 0)
    {
        switch (presentMode)
        {
        case VK_PRESENT_MODE_MAILBOX_KHR:
            min_image_count = 3;
            break;
        case VK_PRESENT_MODE_FIFO_KHR:
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
            min_image_count = 2;
            break;
        default:
            min_image_count = 1;
            break;
        }
    }

    {
        VkSwapchainCreateInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = surface;
        info.minImageCount = min_image_count;
        info.imageFormat = surfaceFormat.format;
        info.imageColorSpace = surfaceFormat.colorSpace;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;           // Assume that graphics family == present family
        info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        info.presentMode = presentMode;
        info.clipped = VK_TRUE;
        info.oldSwapchain = old_swapchain;
        VkSurfaceCapabilitiesKHR cap;
        err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &cap);
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
        err = vkCreateSwapchainKHR(device, &info, allocator, &swapchain);
        check_vk_result(err);
        err = vkGetSwapchainImagesKHR(device, swapchain, &backBufferCount, NULL);
        check_vk_result(err);
        err = vkGetSwapchainImagesKHR(device, swapchain, &backBufferCount, backBuffer);
        check_vk_result(err);
    }
    if (old_swapchain)
    {
        vkDestroySwapchainKHR(device, old_swapchain, allocator);
    }

    {
        VkAttachmentDescription attachment = {};
        attachment.format = surfaceFormat.format;
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentReference color_attachment = {};
        color_attachment.attachment = 0;
        color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_attachment;
        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = 1;
        info.pAttachments = &attachment;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;
        err = vkCreateRenderPass(device, &info, allocator, &renderPass);
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
        for (uint32_t i = 0; i < backBufferCount; i++)
        {
            info.image = backBuffer[i];
            err = vkCreateImageView(device, &info, allocator, &backBufferView[i]);
            check_vk_result(err);
        }
    }
    {
        VkImageView attachment[1];
        VkFramebufferCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderPass;
        info.attachmentCount = 1;
        info.pAttachments = attachment;
        info.width = width;
        info.height = height;
        info.layers = 1;
        for (uint32_t i = 0; i < backBufferCount; i++)
        {
            attachment[0] = backBufferView[i];
            err = vkCreateFramebuffer(device, &info, allocator, &framebuffer[i]);
            check_vk_result(err);
        }
    }
}

uint32_t RendererPrivate::memoryType(VkMemoryPropertyFlags properties, uint32_t type_bits)
{
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1<<i))
            return i;
    return 0xFFFFFFFF; // Unable to find memoryType
}

void RendererPrivate::createOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory, VkDeviceSize& p_buffer_size, size_t new_size, VkBufferUsageFlagBits usage)
{
    static VkDeviceSize g_BufferMemoryAlignment = 256;

    VkResult err;
    if (buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, buffer, allocator);
    if (buffer_memory)
        vkFreeMemory(device, buffer_memory, allocator);

    VkDeviceSize vertex_buffer_size_aligned = ((new_size - 1) / g_BufferMemoryAlignment + 1) * g_BufferMemoryAlignment;
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = vertex_buffer_size_aligned;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    err = vkCreateBuffer(device, &buffer_info, allocator, &buffer);
    check_vk_result(err);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device, buffer, &req);
    g_BufferMemoryAlignment = (g_BufferMemoryAlignment > req.alignment) ? g_BufferMemoryAlignment : req.alignment;
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = memoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
    err = vkAllocateMemory(device, &alloc_info, allocator, &buffer_memory);
    check_vk_result(err);

    err = vkBindBufferMemory(device, buffer, buffer_memory, 0);
    check_vk_result(err);
    p_buffer_size = new_size;
}

void RendererPrivate::renderDrawData(Program * program, Mesh * mesh,
                                     const float4x4 & model, const float4x4 & view, const float4x4 & projection,
                                     const float3 & camera, const float3 & light)
{
    VkCommandBuffer & command_buffer = frames[frameIndex].commandBuffer;

    const ProgramRegistration & programRegistration = registrationFor(programRegistrations, program);
    const MeshRegistration & meshRegistration = registrationFor(meshRegistrations, mesh);

    struct FrameDataForRender
    {
        VkDeviceMemory  VertexBufferMemory;
        VkDeviceMemory  IndexBufferMemory;
        VkDeviceSize    VertexBufferSize;
        VkDeviceSize    IndexBufferSize;
        VkBuffer        VertexBuffer;
        VkBuffer        IndexBuffer;
    };
    FrameDataForRender g_FramesDataBuffers[2] = {};
    FrameDataForRender* fd = &g_FramesDataBuffers[frameIndex];

    // Create the Vertex and Index buffers:
    size_t vertex_size = meshRegistration.vertexBufferSize * sizeof(Vertex);
    size_t index_size = meshRegistration.indexBufferSize * sizeof(unsigned int);
    if (!fd->VertexBuffer || fd->VertexBufferSize < vertex_size)
        createOrResizeBuffer(fd->VertexBuffer, fd->VertexBufferMemory, fd->VertexBufferSize, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    if (!fd->IndexBuffer || fd->IndexBufferSize < index_size)
        createOrResizeBuffer(fd->IndexBuffer, fd->IndexBufferMemory, fd->IndexBufferSize, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    VkResult err;
    // Upload Vertex and index Data:
    {
        Vertex* vtx_dst = NULL;
        unsigned int* idx_dst = NULL;
        err = vkMapMemory(device, fd->VertexBufferMemory, 0, vertex_size, 0, (void**)(&vtx_dst));
        check_vk_result(err);
        err = vkMapMemory(device, fd->IndexBufferMemory, 0, index_size, 0, (void**)(&idx_dst));
        check_vk_result(err);
        {
            memcpy(vtx_dst, meshRegistration.vertices.data(), meshRegistration.vertexBufferSize * sizeof(Vertex));
            memcpy(idx_dst, meshRegistration.indices.data(), meshRegistration.indexBufferSize * sizeof(unsigned int));
            vtx_dst += meshRegistration.vertexBufferSize;
            idx_dst += meshRegistration.indexBufferSize;
        }
        VkMappedMemoryRange range[2] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = fd->VertexBufferMemory;
        range[0].size = VK_WHOLE_SIZE;
        range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[1].memory = fd->IndexBufferMemory;
        range[1].size = VK_WHOLE_SIZE;
        err = vkFlushMappedMemoryRanges(device, 2, range);
        check_vk_result(err);
        vkUnmapMemory(device, fd->VertexBufferMemory);
        vkUnmapMemory(device, fd->IndexBufferMemory);
    }

    // Bind pipeline and descriptor sets:
    {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, programRegistration.pipeline);
        VkDescriptorSet desc_set[1] = { programRegistration.descriptorSet };
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, programRegistration.pipelineLayout, 0, 1, desc_set, 0, NULL);
    }

    // Bind Vertex And Index Buffer:
    {
        VkBuffer vertex_buffers[1] = { fd->VertexBuffer };
        VkDeviceSize vertex_offset[1] = { 0 };
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offset);
        vkCmdBindIndexBuffer(command_buffer, fd->IndexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }

    // Setup viewport:
    {
        VkViewport viewport;
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = width;
        viewport.height = height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 100.0f;
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);
    }

    {
        // maximum push constant size is between 128 and 256... this is 192...
        vkCmdPushConstants(command_buffer, programRegistration.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float4x4) * 0, sizeof(float4x4), &model);
        vkCmdPushConstants(command_buffer, programRegistration.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float4x4) * 1, sizeof(float4x4), &view);
        vkCmdPushConstants(command_buffer, programRegistration.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(float4x4) * 2, sizeof(float4x4), &projection);
        //vkCmdPushConstants(command_buffer, programRegistration.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(float3) * 0, sizeof(float3), &camera);
        //vkCmdPushConstants(command_buffer, programRegistration.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(float3) * 1, sizeof(float3), &light);
    }

    VkRect2D scissor;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.width = width;
    scissor.extent.height = height;
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    // Render the command lists:
    vkCmdDrawIndexed(command_buffer, meshRegistration.indexBufferSize, 1, 0, 0, 0);
}

void RendererPrivate::registerGraphics(Model *model)
{
    model->d = this;
    load(model->filename);
}

void RendererPrivate::registerGraphics(Mesh * mesh)
{
    mesh->d = this;
    const MeshRegistration * meshRegistration = &registrationFor(meshRegistrations, mesh);
    const_cast<MeshRegistration *>(meshRegistration)->referenceCount += 1;
}

void RendererPrivate::registerGraphics(Program * program)
{
    program->d = this;
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
                texture(&textureRegistration.buffer, sibling(*value.texture, meshFile.filename), material.repeatTexcoords);
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
    for (auto * uniform : mesh->uniforms) { delete uniform; }
    mesh->uniforms.clear();
    mesh->d = nullptr;
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
    program->d = nullptr;
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

void RendererPrivate::setProgramUniforms(Mesh * mesh)
{
    const ProgramRegistration & programRegistration = registrationFor(programRegistrations, mesh->program);
    for (const ProgramUniform & programUniform : programRegistration.pipelineReflectionInfo.vertexShaderStage.uniformReflection)
    {
        if (auto * uniform = mesh->uniform(programUniform.name))
        {
            setUniform(textureRegistrations, mesh, uniform, programUniform);
        }
    }
}

void RendererPrivate::setProgramUniform(Program * program, IUniform * uniform)
{
    const ProgramRegistration & programRegistration = registrationFor(programRegistrations, program);
    const auto & uniformReflection = programRegistration.pipelineReflectionInfo.vertexShaderStage.uniformReflection;
    auto programUniform = std::find_if(uniformReflection.begin(), uniformReflection.end(), [uniform](const ProgramUniform & programUniform)
    {
        return programUniform.name == uniform->name;
    });
    if (programUniform != uniformReflection.end())
    {
        setUniform(textureRegistrations, nullptr, uniform, *programUniform);
    }
}

void RendererPrivate::unsetProgramUniform(Program *program, IUniform * uniform)
{
    const ProgramRegistration & programRegistration = registrationFor(programRegistrations, program);
    const auto & uniformReflection = programRegistration.pipelineReflectionInfo.vertexShaderStage.uniformReflection;
    auto programUniform = std::find_if(uniformReflection.begin(), uniformReflection.end(), [uniform](const ProgramUniform & programUniform)
    {
        return programUniform.name == uniform->name;
    });
    if (programUniform != uniformReflection.end())
    {
        //
    }
}

void RendererPrivate::draw(Program *, Mesh *)
{
    //
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
            IUniform * uniform = nullptr;
            if (value.texture)
            {
                uniform = new UniformSampler2D(value.sid, *value.texture);
            }
            else
            {
                uniform = UniformFactory::makeUniform(value.sid, enumForVectorSize(value.data->size()));
                uniform->setData(value.data->data());
            }
            mesh->add(uniform);
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
