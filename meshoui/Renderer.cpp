#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "RendererPrivate.h"
#include "Camera.h"
#include "InputCallbacks.h"
#include "Program.h"
#include "Widget.h"

#include <algorithm>
#include <cstring>
#include <experimental/filesystem>
#include <set>

#include <lodepng.h>
#include <nv_dds.h>

#ifdef MESHOUI_USE_IMGUI
#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>
#endif

using namespace linalg;
using namespace linalg::aliases;
namespace std { namespace filesystem = experimental::filesystem; }
using namespace Meshoui;

namespace
{
    void error_callback(int, const char* description)
    {
        printf("Error: %s\n", description);
    }

    void check_vk_result(VkResult err)
    {
        if (err == 0)
            return;
        printf("VkResult %d\n", err);
        if (err < 0)
            abort();
    }

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

Renderer::~Renderer()
{
    delete widgetCallbacks;
    widgetCallbacks = nullptr;

    GlfwCallbacks::unregister(this);

    // Cleanup
    auto err = vkDeviceWaitIdle(d->device.device);
    check_vk_result(err);
#ifdef MESHOUI_USE_IMGUI
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#endif
    d->swapChain.destroyImageBuffers(d->device, d->depthBuffer);
    d->swapChain.destroyCommandBuffers(d->device);
    vkDestroySurfaceKHR(d->instance.instance, d->surface, d->instance.allocator);
    d->device.destroy();
    d->instance.destroy();

    glfwDestroyWindow(d->window);
    glfwTerminate();

    delete d;
}

Renderer::Renderer()
    : d(new RendererPrivate(this))
    , defaultProgram(nullptr)
    , time(0.f)
#ifdef MESHOUI_USE_IMGUI
    , overlay(true)
#else
    , overlay(false)
#endif
    , cameras()
    , keyboards()
    , mice()
    , meshes()
    , models()
    , programs()
    , widgets()
    , widgetCallbacks(new WidgetCallbacks())
{
    // glfw & vulkan
    glfwSetErrorCallback(error_callback);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    d->window = glfwCreateWindow(1920/2, 1080/2, "Meshoui", nullptr, nullptr);
    if (!glfwVulkanSupported()) printf("GLFW: Vulkan Not Supported\n");
    if (!overlay) glfwSetInputMode(d->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    uint32_t extensionsCount = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensionsCount);
    d->instance.create(extensions, extensionsCount);
    d->device.create(d->instance);

    // Create Window Surface
    VkResult err = glfwCreateWindowSurface(d->instance.instance, d->window, d->device.allocator, &d->surface);
    check_vk_result(err);

    // Create Framebuffers
    int width = 0, height = 0;
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(d->window, &width, &height);
        glfwPostEmptyEvent();
        glfwWaitEvents();
    }

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(d->device.physicalDevice, d->device.queueFamily, d->surface, &res);
    if (res != VK_TRUE)
    {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }
    d->device.selectSurfaceFormat(d->surface, d->surfaceFormat, { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM }, VK_COLORSPACE_SRGB_NONLINEAR_KHR);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    d->swapChain.createCommandBuffers(d->device);
    d->swapChain.createImageBuffers(d->device, d->depthBuffer, d->surface, d->surfaceFormat, width, height, d->toVSync);
    d->toVSync = d->isVSync;
#ifdef MESHOUI_USE_IMGUI
    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(d->window, false);
    {
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = d->instance.instance;
        init_info.PhysicalDevice = d->device.physicalDevice;
        init_info.Device = d->device.device;
        init_info.QueueFamily = d->device.queueFamily;
        init_info.Queue = d->device.queue;
        init_info.PipelineCache = d->pipelineCache;
        init_info.DescriptorPool = d->device.descriptorPool;
        init_info.Allocator = d->device.allocator;
        init_info.CheckVkResultFn = check_vk_result;
        ImGui_ImplVulkan_Init(&init_info, d->swapChain.renderPass);
    }

    ImGui::StyleColorsDark();

    {
        // Use any command queue
        VkCommandPool commandPool = d->swapChain.frames[d->frameIndex].pool;
        VkCommandBuffer commandBuffer = d->swapChain.frames[d->frameIndex].buffer;

        err = vkResetCommandPool(d->device.device, commandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(commandBuffer, &begin_info);
        check_vk_result(err);

        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &commandBuffer;
        err = vkEndCommandBuffer(commandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(d->device.queue, 1, &end_info, VK_NULL_HANDLE);
        check_vk_result(err);

        err = vkDeviceWaitIdle(d->device.device);
        check_vk_result(err);
        ImGui_ImplVulkan_InvalidateFontUploadObjects();
    }
#endif
    glfwSetWindowUserPointer(d->window, this);
    GlfwCallbacks::doregister(this);
}

bool Renderer::shouldClose() const
{
    return glfwWindowShouldClose(d->window);
}

void Renderer::add(SimpleMesh *mesh)
{
    // mesh
    {
        if (mesh->d != nullptr)
            abort();

        MeshPrivate * meshPrivate = mesh->d = new MeshPrivate();
        meshPrivate->indexBufferSize = mesh->geometry.indices.size();
        meshPrivate->definitionId = mesh->geometry.definitionId;

        VkDeviceSize vertex_size = mesh->geometry.vertices.size() * sizeof(Vertex);
        VkDeviceSize index_size = mesh->geometry.indices.size() * sizeof(unsigned int);
        d->device.createBuffer(meshPrivate->vertexBuffer, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        d->device.createBuffer(meshPrivate->indexBuffer, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        d->device.uploadBuffer(meshPrivate->vertexBuffer, vertex_size, mesh->geometry.vertices.data());
        d->device.uploadBuffer(meshPrivate->indexBuffer, index_size, mesh->geometry.indices.data());
    }

    //material
    {
        if (mesh->m != nullptr)
            abort();

        MaterialPrivate * materialPrivate = mesh->m = new MaterialPrivate();
        materialPrivate->materialId = mesh->material.materialId;
        texture(materialPrivate->ambientImage, mesh->material.textureAmbient, float4(mesh->material.ambient, 1.0f), d->device, d->swapChain, d->frameIndex);
        texture(materialPrivate->diffuseImage, mesh->material.textureDiffuse, float4(mesh->material.diffuse, 1.0f), d->device, d->swapChain, d->frameIndex);
        texture(materialPrivate->normalImage, mesh->material.textureNormal, linalg::zero, d->device, d->swapChain, d->frameIndex);
        texture(materialPrivate->emissiveImage, mesh->material.textureEmissive, float4(mesh->material.emissive, 1.0f), d->device, d->swapChain, d->frameIndex);
        texture(materialPrivate->specularImage, mesh->material.textureSpecular, float4(mesh->material.specular, 1.0f), d->device, d->swapChain, d->frameIndex);

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
            err = vkCreateSampler(d->device.device, &info, d->device.allocator, &materialPrivate->ambientSampler);
            check_vk_result(err);
            err = vkCreateSampler(d->device.device, &info, d->device.allocator, &materialPrivate->diffuseSampler);
            check_vk_result(err);
            err = vkCreateSampler(d->device.device, &info, d->device.allocator, &materialPrivate->normalSampler);
            check_vk_result(err);
            err = vkCreateSampler(d->device.device, &info, d->device.allocator, &materialPrivate->specularSampler);
            check_vk_result(err);
            err = vkCreateSampler(d->device.device, &info, d->device.allocator, &materialPrivate->emissiveSampler);
            check_vk_result(err);
        }

        {
            VkDescriptorSetAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool = d->device.descriptorPool;
            alloc_info.descriptorSetCount = 1;
            alloc_info.pSetLayouts = &defaultProgram->d->descriptorSetLayout[MESHOUI_MATERIAL_DESC_LAYOUT];
            err = vkAllocateDescriptorSets(d->device.device, &alloc_info, &materialPrivate->descriptorSet);
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
            vkUpdateDescriptorSets(d->device.device, 5, write_desc, 0, nullptr);
        }
    }
}

void Renderer::add(Model *model)
{
    if (std::find(models.begin(), models.end(), model) == models.end())
    {
        models.push_back(model);
        d->registerGraphics(model);
    }
}

void Renderer::add(Mesh * mesh)
{
    if (std::find(meshes.begin(), meshes.end(), mesh) == meshes.end())
    {
        meshes.push_back(mesh);
        if (!mesh->program)
            mesh->program = defaultProgram;
        d->registerGraphics(mesh);
    }
}

void Renderer::add(Program * program)
{
    if (std::find(programs.begin(), programs.end(), program) == programs.end())
    {
        programs.push_back(program);
        d->registerGraphics(program);
    }
    if (defaultProgram == nullptr)
        defaultProgram = program;
}

void Renderer::add(Camera * camera)
{
    if (std::find(cameras.begin(), cameras.end(), camera) == cameras.end())
    {
        cameras.push_back(camera);
        d->registerGraphics(camera);
    }
}

void Renderer::add(Widget * widget)
{
    if (std::find(widgets.begin(), widgets.end(), widget) == widgets.end())
    {
        widgets.push_back(widget);
    }
}

void Renderer::add(IKeyboard *keyboard)
{
    if (std::find(keyboards.begin(), keyboards.end(), keyboard) == keyboards.end())
    {
        keyboards.push_back(keyboard);
    }
}

void Renderer::add(IMouse *mouse)
{
    if (std::find(mice.begin(), mice.end(), mouse) == mice.end())
    {
        mice.push_back(mouse);
    }
}

void Renderer::remove(Model *model)
{
    d->unregisterGraphics(model);
    models.erase(std::remove(models.begin(), models.end(), model));
}

void Renderer::remove(Mesh* mesh)
{
    d->unregisterGraphics(mesh);
    if (mesh->program == defaultProgram)
        mesh->program = nullptr;
    meshes.erase(std::remove(meshes.begin(), meshes.end(), mesh));
}

void Renderer::remove(Program* program)
{
    d->unregisterGraphics(program);
    programs.erase(std::remove(programs.begin(), programs.end(), program));
    if (defaultProgram == program)
        defaultProgram = nullptr;
}

void Renderer::remove(Camera* camera)
{
    d->unregisterGraphics(camera);
    cameras.erase(std::remove(cameras.begin(), cameras.end(), camera));
}

void Renderer::remove(Widget * widget)
{
    widgets.erase(std::remove(widgets.begin(), widgets.end(), widget));
}

void Renderer::remove(IKeyboard *keyboard)
{
    keyboards.erase(std::remove(keyboards.begin(), keyboards.end(), keyboard));
}

void Renderer::remove(IMouse *mouse)
{
    mice.erase(std::remove(mice.begin(), mice.end(), mouse));
}

void Renderer::update(float s)
{
    glfwPollEvents();

    VkSemaphore &imageAcquiredSemaphore  = d->swapChain.beginRender(d->device, d->frameIndex);

    renderMeshes();
    renderWidgets();

    VkResult err = d->swapChain.endRender(imageAcquiredSemaphore, d->device.queue, d->frameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || (d->toVSync != d->isVSync))
    {
        int width = 0, height = 0;
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(d->window, &width, &height);
            glfwPostEmptyEvent();
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(d->device.device);
        d->swapChain.createImageBuffers(d->device, d->depthBuffer, d->surface, d->surfaceFormat, width, height, d->toVSync);
        d->isVSync = d->toVSync;
    }
    else
    {
        check_vk_result(err);
    }

    postUpdate();

    time += s;
}

void Renderer::postUpdate()
{
    if (d->toFullscreen && !d->isFullscreen)
    {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(d->window, glfwGetPrimaryMonitor(), 0, 0, 1920, 1080, GLFW_DONT_CARE);
        for (auto * cb : mice) { cb->mouseLost(); }
    }
    else if (!d->toFullscreen && d->isFullscreen)
    {
        glfwSetWindowMonitor(d->window, nullptr, 80, 80, 1920/2, 1080/2, GLFW_DONT_CARE);
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
        for (auto * cb : mice) { cb->mouseLost(); }
    }
    d->isFullscreen = d->toFullscreen;
}

void Renderer::renderMeshes()
{
    auto & frame = d->swapChain.frames[d->frameIndex];

    VkViewport viewport{0, 0, float(d->swapChain.extent.width), float(d->swapChain.extent.height), 0.f, 1.f};
    vkCmdSetViewport(frame.buffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {d->swapChain.extent.width, d->swapChain.extent.height}};
    vkCmdSetScissor(frame.buffer, 0, 1, &scissor);

    d->uniforms.position = d->camera->modelMatrix.w.xyz();
    d->uniforms.light = d->lights[0]->modelMatrix.w.xyz();

    Program * currentProgram = nullptr;

    std::stable_sort(meshes.begin(), meshes.end(), [](Mesh *, Mesh * right) { return (right->renderFlags & Render::DepthWrite) == 0; });
    for (Mesh * mesh : meshes)
    {
        if ((mesh->renderFlags & Render::Visible) == 0)
            continue;

        if (currentProgram != mesh->program)
        {
            currentProgram = mesh->program;
            d->bindGraphics(currentProgram);
        }
        d->bindGraphics(mesh);

        d->pushConstants.model = mesh->modelMatrix;
        if (d->camera != nullptr)
            d->pushConstants.view = d->camera->viewMatrix(mesh->viewFlags);
        d->pushConstants.projection = mesh->viewFlags == View::None ? identity : d->projectionMatrix;

        d->draw(mesh);
        d->unbindGraphics(mesh);
    }

    d->unbindGraphics(currentProgram);
}

void Renderer::renderWidgets()
{
    if (overlay)
    {
#ifdef MESHOUI_USE_IMGUI
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Main window");
        ImGui::Text("Press ESCAPE (Esc) to switch to mouselook");
        if (ImGui::CollapsingHeader("Menu", ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (ImGui::Button("Exit"))
            {
                glfwSetWindowShouldClose(d->window, 1);
            }
        }
        if (ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("fullscreen", &d->toFullscreen);
            ImGui::Checkbox("Vsync", &d->toVSync);
        }
        for (auto * widget : widgets)
        {
            if (widget->window == "Main window")
                widget->draw();
        }
        if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("frametime : %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Text("time : %.3f s", time);
            ImGui::Text("meshes : %zu mesh(es), %zu file(s)", meshes.size(), d->meshFiles.size());
            if (ImGui::Button("Clear cache"))
                d->meshFiles.clear();
            if (ImGui::CollapsingHeader("instances"))
            {
                for (const auto * mesh : meshes)
                {
                    ImGui::Text("%s", mesh->instanceId.str.c_str());
                }
            }
            if (ImGui::CollapsingHeader("files"))
            {
                for (const auto & meshFile : d->meshFiles)
                {
                    ImGui::Text("%s", meshFile.filename.c_str());
                }
            }
        }
        ImGui::End();

        for (auto * widget : widgets)
        {
            if (widget->window != "Main window")
                widget->draw();
        }

        // Rendering
        ImGui::Render();

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), d->swapChain.frames[d->frameIndex].buffer);
#else
        glfwSetWindowShouldClose(d->window, 1);
#endif
    }
}
