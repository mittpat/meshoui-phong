#include "InputCallbacks.h"
#include "Renderer.h"
#include "RendererPrivate.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef MESHOUI_USE_IMGUI
#include <imgui.h>
#include <examples/imgui_impl_glfw.h>
#include <examples/imgui_impl_vulkan.h>
#endif

using namespace Meshoui;

void WidgetCallbacks::mouseButtonAction(void *window, int button, int action, int mods, bool overlay)
{
#ifdef MESHOUI_USE_IMGUI
    if (overlay) ImGui_ImplGlfw_MouseButtonCallback(reinterpret_cast<GLFWwindow*>(window), button, action, mods);
#endif
}

void WidgetCallbacks::scrollAction(void *window, double xoffset, double yoffset, bool overlay)
{
#ifdef MESHOUI_USE_IMGUI
    if (overlay) ImGui_ImplGlfw_ScrollCallback(reinterpret_cast<GLFWwindow*>(window), xoffset, yoffset);
#endif
}

void WidgetCallbacks::keyAction(void *window, int key, int scancode, int action, int mods)
{
#ifdef MESHOUI_USE_IMGUI
    ImGui_ImplGlfw_KeyCallback(reinterpret_cast<GLFWwindow*>(window), key, scancode, action, mods);
#endif
}

void WidgetCallbacks::charAction(void *window, unsigned int c)
{
#ifdef MESHOUI_USE_IMGUI
    ImGui_ImplGlfw_CharCallback(reinterpret_cast<GLFWwindow*>(window), c);
#endif
}

void GlfwCallbacks::doregister(Renderer *renderer)
{
    glfwSetCursorEnterCallback(renderer->d->window, GlfwCallbacks::cursorEnterCallback);
    glfwSetCursorPosCallback(renderer->d->window, GlfwCallbacks::cursorPositionCallback);
    glfwSetMouseButtonCallback(renderer->d->window, GlfwCallbacks::mouseButtonCallback);
    glfwSetScrollCallback(renderer->d->window, GlfwCallbacks::scrollCallback);
    glfwSetKeyCallback(renderer->d->window, GlfwCallbacks::keyCallback);
    glfwSetCharCallback(renderer->d->window, GlfwCallbacks::charCallback);
}

void GlfwCallbacks::unregister(Renderer *renderer)
{
    glfwSetCursorEnterCallback(renderer->d->window, nullptr);
    glfwSetCursorPosCallback(renderer->d->window, nullptr);
    glfwSetMouseButtonCallback(renderer->d->window, nullptr);
    glfwSetScrollCallback(renderer->d->window, nullptr);
    glfwSetKeyCallback(renderer->d->window, nullptr);
    glfwSetCharCallback(renderer->d->window, nullptr);
}

void GlfwCallbacks::cursorEnterCallback(GLFWwindow *window, int)
{
    Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    for (auto * cb : renderer->mice) { cb->mouseLost(); }
}

void GlfwCallbacks::cursorPositionCallback(GLFWwindow *window, double xpos, double ypos)
{
    Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->widgetCallbacks->cursorPositionAction(window, xpos, ypos, renderer->overlay);
#ifdef MESHOUI_USE_IMGUI
    if (renderer->overlay)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
        {
            for (auto * cb : renderer->mice) { cb->mouseLost(); }
            return;
        }
    }
#endif
    for (auto * cb : renderer->mice) { cb->cursorPositionAction(window, xpos, ypos, renderer->overlay); }
}

void GlfwCallbacks::mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
{
    Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->widgetCallbacks->mouseButtonAction(window, button, action, mods, renderer->overlay);
#ifdef MESHOUI_USE_IMGUI
    if (renderer->overlay)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
        {
            for (auto * cb : renderer->mice) { cb->mouseLost(); }
            return;
        }
    }
#endif
    for (auto * cb : renderer->mice) { cb->mouseButtonAction(window, button, action, mods, renderer->overlay); }
}

void GlfwCallbacks::scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->widgetCallbacks->scrollAction(window, xoffset, yoffset, renderer->overlay);
#ifdef MESHOUI_USE_IMGUI
    if (renderer->overlay)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureMouse)
        {
            for (auto * cb : renderer->mice) { cb->mouseLost(); }
            return;
        }
    }
#endif
    for (auto * cb : renderer->mice) { cb->scrollAction(window, xoffset, yoffset, renderer->overlay); }
}

void GlfwCallbacks::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->widgetCallbacks->keyAction(window, key, scancode, action, mods);
#ifdef MESHOUI_USE_IMGUI
    if (renderer->overlay)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard)
        {
            for (auto * cb : renderer->keyboards) { cb->keyboardLost(); }
            return;
        }
    }
#endif
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE)
    {
        auto * wind = reinterpret_cast<GLFWwindow*>(window);
        Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(wind));
        renderer->overlay = !renderer->overlay;
        glfwSetInputMode(wind, GLFW_CURSOR, renderer->overlay ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
        for (auto * cb : renderer->mice) { cb->mouseLost(); }
    }
    if (action == GLFW_PRESS && key == GLFW_KEY_F11)
    {
        renderer->d->toFullscreen = !renderer->d->toFullscreen;
    }
    for (auto * cb : renderer->keyboards) { cb->keyAction(window, key, scancode, action, mods); }
}

void GlfwCallbacks::charCallback(GLFWwindow *window, unsigned int c)
{
    Renderer * renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->widgetCallbacks->charAction(window, c);
#ifdef MESHOUI_USE_IMGUI
    if (renderer->overlay)
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantCaptureKeyboard)
        {
            for (auto * cb : renderer->keyboards) { cb->keyboardLost(); }
            return;
        }
    }
#endif
    for (auto * cb : renderer->keyboards) { cb->charAction(window, c); }
}
