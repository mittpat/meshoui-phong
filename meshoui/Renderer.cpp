#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "RendererPrivate.h"
#include "Camera.h"
#include "Program.h"
#include "Widget.h"

#include <loose.h>

#include <algorithm>
#include <set>

#define USE_IMGUI
#ifdef USE_IMGUI
#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>
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

struct Meshoui::Renderer::GlfwCallbacks
{
    static void doregister(Renderer* renderer)
    {
        glfwSetMouseButtonCallback(renderer->d->window, GlfwCallbacks::mouseButtonCallback);
        glfwSetScrollCallback(renderer->d->window, GlfwCallbacks::scrollCallback);
        glfwSetKeyCallback(renderer->d->window, GlfwCallbacks::keyCallback);
        glfwSetCharCallback(renderer->d->window, GlfwCallbacks::charCallback);
    }
    static void unregister(Renderer* renderer)
    {
        glfwSetMouseButtonCallback(renderer->d->window, nullptr);
        glfwSetScrollCallback(renderer->d->window, nullptr);
        glfwSetKeyCallback(renderer->d->window, nullptr);
        glfwSetCharCallback(renderer->d->window, nullptr);
    }
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
    {
        Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        for (auto * cb : renderer->mice)
        {
            cb->mouseButtonAction(window, button, action, mods);
        }
    }
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
    {
        Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        for (auto * cb : renderer->mice)
        {
            cb->scrollAction(window, xoffset, yoffset);
        }
    }
    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        for (auto * cb : renderer->keyboards)
        {
            cb->keyAction(window, key, scancode, action, mods);
        }
    }
    static void charCallback(GLFWwindow* window, unsigned int c)
    {
        Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        for (auto * cb : renderer->keyboards)
        {
            cb->charAction(window, c);
        }
    }
};

struct Meshoui::Renderer::WidgetCallbacks
    : IKeyboard
    , IMouse
{
    virtual void mouseButtonAction(void *window, int button, int action, int mods) override
    {
#ifdef USE_IMGUI
        ImGui_ImplGlfw_MouseButtonCallback(reinterpret_cast<GLFWwindow*>(window), button, action, mods);
#endif
    }
    virtual void scrollAction(void *window, double xoffset, double yoffset) override
    {
#ifdef USE_IMGUI
        ImGui_ImplGlfw_ScrollCallback(reinterpret_cast<GLFWwindow*>(window), xoffset, yoffset);
#endif
    }
    virtual void keyAction(void *window, int key, int scancode, int action, int mods) override
    {
#ifdef USE_IMGUI
        ImGui_ImplGlfw_KeyCallback(reinterpret_cast<GLFWwindow*>(window), key, scancode, action, mods);
#endif
    }
    virtual void charAction(void *window, unsigned int c) override
    {
#ifdef USE_IMGUI
        ImGui_ImplGlfw_CharCallback(reinterpret_cast<GLFWwindow*>(window), c);
#endif
    }
};

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

Renderer::~Renderer()
{
    remove((IKeyboard*)widgetCallbacks);
    remove((IMouse*)widgetCallbacks);
    delete widgetCallbacks;
    widgetCallbacks = nullptr;

    GlfwCallbacks::unregister(this);

    // Cleanup
    auto err = vkDeviceWaitIdle(d->renderDevice.device);
    check_vk_result(err);
#ifdef USE_IMGUI
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
#endif
    d->destroySwapChainAndFramebuffer();
    d->destroyCommandBuffers();
    d->destroyGraphicsSubsystem();

    //vkDestroySurfaceKHR(d->instance, d->surface, d->renderDevice.allocator);
    glfwDestroyWindow(d->window);
    glfwTerminate();

    delete d;
}

Renderer::Renderer()
    : d(new RendererPrivate)
    , defaultProgram(nullptr)
    , time(0.f)
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
    if (!glfwVulkanSupported())
    {
        printf("GLFW: Vulkan Not Supported\n");
    }

    glfwSetWindowUserPointer(d->window, this);
    GlfwCallbacks::doregister(this);
    add((IKeyboard*)widgetCallbacks);
    add((IMouse*)widgetCallbacks);

    uint32_t extensions_count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&extensions_count);    
    d->createGraphicsSubsystem(extensions, extensions_count);

    // Create Window Surface
    VkResult err = glfwCreateWindowSurface(d->instance, d->window, d->renderDevice.allocator, &d->surface);
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
    vkGetPhysicalDeviceSurfaceSupportKHR(d->renderDevice.physicalDevice, d->queueFamily, d->surface, &res);
    if (res != VK_TRUE)
    {
        fprintf(stderr, "Error no WSI support on physical device 0\n");
        exit(-1);
    }
    d->renderDevice.selectSurfaceFormat(d->surface, d->surfaceFormat, { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM }, VK_COLORSPACE_SRGB_NONLINEAR_KHR);

    // Create SwapChain, RenderPass, Framebuffer, etc.
    d->createCommandBuffers();
    d->createSwapChainAndFramebuffer(width, height, d->toVSync);
    d->toVSync = d->isVSync;
#ifdef USE_IMGUI
    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(d->window, false);
    {
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = d->instance;
        init_info.PhysicalDevice = d->renderDevice.physicalDevice;
        init_info.Device = d->renderDevice.device;
        init_info.QueueFamily = d->queueFamily;
        init_info.Queue = d->queue;
        init_info.PipelineCache = d->pipelineCache;
        init_info.DescriptorPool = d->descriptorPool;
        init_info.Allocator = d->renderDevice.allocator;
        init_info.CheckVkResultFn = check_vk_result;
        ImGui_ImplVulkan_Init(&init_info, d->renderPass);
    }

    ImGui::StyleColorsDark();

    {
        // Use any command queue
        VkCommandPool command_pool = d->swapChain.frames[d->frameIndex].pool;
        VkCommandBuffer command_buffer = d->swapChain.frames[d->frameIndex].buffer;

        err = vkResetCommandPool(d->renderDevice.device, command_pool, 0);
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

        err = vkDeviceWaitIdle(d->renderDevice.device);
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

    VkResult err;

    VkSemaphore& image_acquired_semaphore  = d->swapChain.frames[d->frameIndex].acquired;
    err = vkAcquireNextImageKHR(d->renderDevice.device, d->swapChainKHR, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &d->frameIndex);
    check_vk_result(err);

    {
        err = vkWaitForFences(d->renderDevice.device, 1, &d->swapChain.frames[d->frameIndex].fence, VK_TRUE, UINT64_MAX);	// wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(d->renderDevice.device, 1, &d->swapChain.frames[d->frameIndex].fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(d->renderDevice.device, d->swapChain.frames[d->frameIndex].pool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(d->swapChain.frames[d->frameIndex].buffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = d->renderPass;
        info.framebuffer = d->swapChain.images[d->frameIndex].front;
        info.renderArea.extent.width = d->width;
        info.renderArea.extent.height = d->height;
        VkClearValue clearValue[2] = {0};
        clearValue[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
        clearValue[1].depthStencil = {1.0f, 0};
        info.pClearValues = clearValue;
        info.clearValueCount = 2;
        vkCmdBeginRenderPass(d->swapChain.frames[d->frameIndex].buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    renderMeshes();
    renderWidgets();

    vkCmdEndRenderPass(d->swapChain.frames[d->frameIndex].buffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &d->swapChain.frames[d->frameIndex].buffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &d->swapChain.frames[d->frameIndex].complete;

        VkResult err = vkEndCommandBuffer(d->swapChain.frames[d->frameIndex].buffer);
        check_vk_result(err);
        err = vkQueueSubmit(d->queue, 1, &info, d->swapChain.frames[d->frameIndex].fence);
        check_vk_result(err);
    }

    {
        VkPresentInfoKHR info = {};
        info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &d->swapChain.frames[d->frameIndex].complete;
        info.swapchainCount = 1;
        info.pSwapchains = &d->swapChainKHR;
        info.pImageIndices = &d->frameIndex;
        VkResult err = vkQueuePresentKHR(d->queue, &info);
        if (err == VK_ERROR_OUT_OF_DATE_KHR || (d->toVSync != d->isVSync))
        {
            int width = 0, height = 0;
            while (width == 0 || height == 0)
            {
                glfwGetFramebufferSize(d->window, &width, &height);
                glfwPostEmptyEvent();
                glfwWaitEvents();
            }

            vkDeviceWaitIdle(d->renderDevice.device);
            d->createSwapChainAndFramebuffer(width, height, d->toVSync);
            d->isVSync = d->toVSync;
        }
        else
        {
            check_vk_result(err);
        }
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
    }
    else if (!d->toFullscreen && d->isFullscreen)
    {
        glfwSetWindowMonitor(d->window, nullptr, 80, 80, 1920/2, 1080/2, GLFW_DONT_CARE);
        glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
    }
    d->isFullscreen = d->toFullscreen;
}

void Renderer::renderMeshes()
{
    auto & frame = d->swapChain.frames[d->frameIndex];

    VkViewport viewport{0, 0, float(d->width), float(d->height), 0.f, 1.f};
    vkCmdSetViewport(frame.buffer, 0, 1, &viewport);

    VkRect2D scissor{0, 0, d->width, d->height};
    vkCmdSetScissor(frame.buffer, 0, 1, &scissor);

    d->uniforms.position = d->camera->position;
    d->uniforms.light = d->lights[0]->position;

    Program * currentProgram = nullptr;

    std::stable_sort(meshes.begin(), meshes.end(), [](Mesh *, Mesh * right) { return (right->renderFlags & Render::DepthWrite) == 0; });
    for (Mesh * mesh : meshes)
    {
        if ((mesh->renderFlags & Render::Visible) == 0)
            continue;

        if (currentProgram != mesh->program)
        {
            currentProgram = mesh->program;
            d->bindGraphics(mesh->program);
        }
        d->bindGraphics(mesh);

        d->pushConstants.model = mesh->modelMatrix();
        if (d->camera != nullptr)
            d->pushConstants.view = d->camera->viewMatrix(mesh->viewFlags);
        d->pushConstants.projection = mesh->viewFlags == View::None ? identity : d->projectionMatrix;

        mesh->program->draw(mesh);
        d->unbindGraphics(mesh);
    }

    d->unbindGraphics(currentProgram);
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

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), d->swapChain.frames[d->frameIndex].buffer);
#endif
}
