#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "RendererPrivate.h"
#include "Camera.h"
#include "Program.h"
#include "Uniform.h"
#include "Widget.h"

#include <loose.h>

#include <algorithm>
#include <set>

#define USE_IMGUI
#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#else
#define IM_ARRAYSIZE(_ARR)          ((int)(sizeof(_ARR)/sizeof(*_ARR)))         // Size of a static C-style array. Don't use on pointers!
#endif

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
}

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

Renderer::~Renderer()
{
    // Cleanup
    auto err = vkDeviceWaitIdle(d->device);
    check_vk_result(err);
#ifdef USE_IMGUI
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#endif
    d->destroySwapChainAndFramebuffer();
    d->destroyCommandBuffers();
    d->destroyGraphicsSubsystem();

    vkDestroySurfaceKHR(d->instance, d->surface, d->allocator);
    glfwDestroyWindow(d->window);
    glfwTerminate();

    delete d;
}

Renderer::Renderer()
    : d(new RendererPrivate)
    , defaultProgram(nullptr)
    , time(0.f)
    , meshes()
    , programs()
{
    // glfw & vulkan
    glfwSetErrorCallback(error_callback);
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    d->window = glfwCreateWindow(1920/2, 1080/2, "Meshoui", nullptr, nullptr);
    if (!glfwVulkanSupported())
    {
        printf("GLFW: Vulkan Not Supported\n");
    }
    uint32_t extensions_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);    
    d->createGraphicsSubsystem(extensions, extensions_count);

    // Create Window Surface
    VkResult err = glfwCreateWindowSurface(d->instance, d->window, d->allocator, &d->surface);
    check_vk_result(err);

    // Create Framebuffers
    int width = 0, height = 0;
    while (width == 0 || height == 0)
    {
        glfwGetFramebufferSize(d->window, &width, &height);
        glfwWaitEvents();
    }

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(d->physicalDevice, d->queueFamily, d->surface, &res);
    if (res != VK_TRUE)
    {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    d->selectSurfaceFormat(requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    d->selectPresentMode(&present_modes[0], IM_ARRAYSIZE(present_modes));

    // Create SwapChain, RenderPass, Framebuffer, etc.
    d->createCommandBuffers();
    d->createSwapChainAndFramebuffer(width, height);
#ifdef USE_IMGUI
    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(d->window, true);
    {
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = d->instance;
        init_info.PhysicalDevice = d->physicalDevice;
        init_info.Device = d->device;
        init_info.QueueFamily = d->queueFamily;
        init_info.Queue = d->queue;
        init_info.PipelineCache = d->pipelineCache;
        init_info.DescriptorPool = d->descriptorPool;
        init_info.Allocator = d->allocator;
        init_info.CheckVkResultFn = check_vk_result;
        ImGui_ImplVulkan_Init(&init_info, d->renderPass);
    }

    ImGui::StyleColorsDark();

    {
        // Use any command queue
        VkCommandPool command_pool = d->frames[d->frameIndex].commandPool;
        VkCommandBuffer command_buffer = d->frames[d->frameIndex].commandBuffer;

        err = vkResetCommandPool(d->device, command_pool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(command_buffer, &begin_info);
        check_vk_result(err);

        ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &command_buffer;
        err = vkEndCommandBuffer(command_buffer);
        check_vk_result(err);
        err = vkQueueSubmit(d->queue, 1, &end_info, VK_NULL_HANDLE);
        check_vk_result(err);

        err = vkDeviceWaitIdle(d->device);
        check_vk_result(err);
        ImGui_ImplVulkan_InvalidateFontUploadObjects();
    }
#endif
}

bool Renderer::shouldClose() const
{
    return glfwWindowShouldClose(d->window);
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

void Renderer::update(float s)
{
    glfwPollEvents();

    renderMeshes();
    renderWidgets();

    postUpdate();

    time += s;
}

void Renderer::postUpdate()
{
    if (d->toFullscreen && !d->fullscreen)
    {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(d->window, glfwGetPrimaryMonitor(), 0, 0, 1920, 1080, GLFW_DONT_CARE);
    }
    else if (!d->toFullscreen && d->fullscreen)
    {
        glfwSetWindowMonitor(d->window, nullptr, 80, 80, 1920/2, 1080/2, GLFW_DONT_CARE);
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    }
    d->fullscreen = d->toFullscreen;
}

void Renderer::renderMeshes()
{
    std::stable_sort(meshes.begin(), meshes.end(), [](Mesh *, Mesh * right) { return (right->renderFlags & Render::DepthWrite) == 0; });
    for (Mesh * mesh : meshes)
    {
        if ((mesh->renderFlags & Render::Visible) == 0)
            continue;

        if (!d->lights.empty())
        {
            if (auto uniform = dynamic_cast<Uniform3fv *>(mesh->program->uniform("sunPosition")))
                uniform->value = normalize(d->lights[0]->position);
            if (auto uniform = dynamic_cast<Uniform3fv *>(mesh->program->uniform("uniformLightPosition")))
                uniform->value = d->lights[0]->position;
        }

        if (d->camera != nullptr)
        {
            if (auto uniform = dynamic_cast<Uniform44fm*>(mesh->program->uniform("uniformView")))
                uniform->value = d->camera->viewMatrix(mesh->viewFlags);
            if (auto uniform = dynamic_cast<Uniform3fv*>(mesh->program->uniform("uniformViewPosition")))
                uniform->value = d->camera->position;
        }

        if (auto uniform = dynamic_cast<Uniform44fm*>(mesh->program->uniform("uniformModel")))
            uniform->value = mesh->modelMatrix();
        if (auto uniform = dynamic_cast<Uniform44fm*>(mesh->program->uniform("uniformProjection")))
            uniform->value = mesh->viewFlags == View::None ? identity : d->projectionMatrix;

        if (auto uniform = dynamic_cast<Uniform2fv*>(mesh->program->uniform("uniformTime")))
            uniform->value = float2(time, 0.016f);

        d->bindGraphics(mesh->program);
        mesh->program->applyUniforms();
        mesh->applyUniforms();
        d->bindGraphics(mesh);
        mesh->program->draw(mesh);
        d->unbindGraphics(mesh);
        mesh->unapplyUniforms();
        mesh->program->unapplyUniforms();
        d->unbindGraphics(mesh->program);
    }
}

void Renderer::renderWidgets()
{
#ifdef USE_IMGUI
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Main window");
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
    }
    for (auto * widget : widgets)
    {
        if (widget->window == "Main window")
            widget->draw();
    }
    if (ImGui::CollapsingHeader("Debug", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("frametime : %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::Text("meshes : %zu instance(s), %zu definition(s), %zu file(s)", meshes.size(), d->meshRegistrations.size(), d->meshFiles.size());
        if (ImGui::Button("Clear cache"))
            d->meshFiles.clear();
        if (ImGui::CollapsingHeader("instances"))
        {
            for (const auto * mesh : meshes)
            {
                ImGui::Text("%s", mesh->instanceId.str.c_str());
            }
        }
        if (ImGui::CollapsingHeader("definitions"))
        {
            for (const auto & registration : d->meshRegistrations)
            {
                ImGui::Text("%s", registration.definitionId.str.c_str());
            }
        }
        if (ImGui::CollapsingHeader("textures"))
        {
            for (const auto & registration : d->textureRegistrations)
            {
                ImGui::Text("%s", registration.name.str.c_str());
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

    {
        VkResult err;

        VkSemaphore& image_acquired_semaphore  = d->frames[d->frameIndex].imageAcquiredSemaphore;
        err = vkAcquireNextImageKHR(d->device, d->swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &d->frameIndex);
        check_vk_result(err);

        {
            err = vkWaitForFences(d->device, 1, &d->frames[d->frameIndex].fence, VK_TRUE, UINT64_MAX);	// wait indefinitely instead of periodically checking
            check_vk_result(err);

            err = vkResetFences(d->device, 1, &d->frames[d->frameIndex].fence);
            check_vk_result(err);
        }
        {
            err = vkResetCommandPool(d->device, d->frames[d->frameIndex].commandPool, 0);
            check_vk_result(err);
            VkCommandBufferBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(d->frames[d->frameIndex].commandBuffer, &info);
            check_vk_result(err);
        }
        {
            VkRenderPassBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            info.renderPass = d->renderPass;
            info.framebuffer = d->framebuffer[d->frameIndex];
            info.renderArea.extent.width = d->width;
            info.renderArea.extent.height = d->height;
            info.clearValueCount = 1;

            static const VkClearValue ClearValue = {0};
            info.pClearValues = &ClearValue;
            vkCmdBeginRenderPass(d->frames[d->frameIndex].commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
        }

        // Record Imgui Draw Data and draw funcs into command buffer
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), d->frames[d->frameIndex].commandBuffer);

        // Submit command buffer
        vkCmdEndRenderPass(d->frames[d->frameIndex].commandBuffer);
        {
            VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            VkSubmitInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            info.waitSemaphoreCount = 1;
            info.pWaitSemaphores = &image_acquired_semaphore;
            info.pWaitDstStageMask = &wait_stage;
            info.commandBufferCount = 1;
            info.pCommandBuffers = &d->frames[d->frameIndex].commandBuffer;
            info.signalSemaphoreCount = 1;
            info.pSignalSemaphores = &d->frames[d->frameIndex].renderCompleteSemaphore;

            err = vkEndCommandBuffer(d->frames[d->frameIndex].commandBuffer);
            check_vk_result(err);
            err = vkQueueSubmit(d->queue, 1, &info, d->frames[d->frameIndex].fence);
            check_vk_result(err);
        }
    }

    {
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &d->frames[d->frameIndex].renderCompleteSemaphore;
        info.swapchainCount = 1;
        info.pSwapchains = &d->swapchain;
        info.pImageIndices = &d->frameIndex;
        VkResult err = vkQueuePresentKHR(d->queue, &info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR)
        {
            int width = 0, height = 0;
            while (width == 0 || height == 0)
            {
                glfwGetFramebufferSize(d->window, &width, &height);
                glfwWaitEvents();
            }

            vkDeviceWaitIdle(d->device);
            d->createSwapChainAndFramebuffer(width, height);
        }
        else
        {
            check_vk_result(err);
        }
    }
#endif
}
