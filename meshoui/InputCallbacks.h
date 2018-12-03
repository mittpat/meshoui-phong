#pragma once

#include "IKeyboard.h"
#include "IMouse.h"

struct GLFWwindow;
namespace Meshoui
{
    struct WidgetCallbacks final
        : IKeyboard
        , IMouse
    {
        virtual void mouseButtonAction(void *window, int button, int action, int mods, bool overlay) override;
        virtual void scrollAction(void *window, double xoffset, double yoffset, bool overlay) override;
        virtual void keyAction(void *window, int key, int scancode, int action, int mods) override;
        virtual void charAction(void *window, unsigned int c) override;
    };

    class Renderer;
    struct GlfwCallbacks final
    {
        static void doregister(Renderer* renderer);
        static void unregister(Renderer* renderer);
        static void cursorEnterCallback(GLFWwindow* window, int);
        static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
        static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void charCallback(GLFWwindow* window, unsigned int c);
    };
}
