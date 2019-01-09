#include "control.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>

static GLFWwindow *g_window = nullptr;
static GLFWcursorenterfun g_previous_cursorenterfun = nullptr;
static GLFWcursorposfun   g_previous_cursorposfun   = nullptr;
static GLFWmousebuttonfun g_previous_mousebuttonfun = nullptr;
static GLFWscrollfun      g_previous_scrollfun      = nullptr;
static GLFWkeyfun         g_previous_keyfun         = nullptr;
static GLFWcharfun        g_previous_charfun        = nullptr;

namespace GlfwCallbacks
{
    class IKeyboard
    {
    public:
        virtual ~IKeyboard() {}
        virtual void keyAction(void* /*window*/, int /*key*/, int /*scancode*/, int /*action*/, int /*mods*/) {}
        virtual void charAction(void* /*window*/, unsigned int /*c*/) {}
    };

    class IMouse
    {
    public:
        virtual ~IMouse() {}
        virtual void cursorPositionAction(void* /*window*/, double /*xpos*/, double /*ypos*/) {}
        virtual void mouseButtonAction(void* /*window*/, int /*button*/, int /*action*/, int /*mods*/) {}
        virtual void scrollAction(void* /*window*/, double /*xoffset*/, double /*yoffset*/) {}
        virtual void mouseLost() {}
    };

    std::vector<IKeyboard *> keyboards;
    std::vector<IMouse *> mice;

    void cursorEnterCallback(GLFWwindow *window, int entered)
    {
        if (g_previous_cursorenterfun) { g_previous_cursorenterfun(window, entered); }
        for (auto * cb : mice) { cb->mouseLost(); }
    }

    void cursorPositionCallback(GLFWwindow *window, double xpos, double ypos)
    {
        if (g_previous_cursorposfun) { g_previous_cursorposfun(window, xpos, ypos); }
        for (auto * cb : mice) { cb->cursorPositionAction(window, xpos, ypos); }
    }

    void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
    {
        if (g_previous_mousebuttonfun) { g_previous_mousebuttonfun(window, button, action, mods); }
        for (auto * cb : mice) { cb->mouseButtonAction(window, button, action, mods); }
    }

    void scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
    {
        if (g_previous_scrollfun) { g_previous_scrollfun(window, xoffset, yoffset); }
        for (auto * cb : mice) { cb->scrollAction(window, xoffset, yoffset); }
    }

    void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
    {
        if (g_previous_keyfun) { g_previous_keyfun(window, key, scancode, action, mods); }
        for (auto * cb : keyboards) { cb->keyAction(window, key, scancode, action, mods); }
    }

    void charCallback(GLFWwindow *window, unsigned int c)
    {
        if (g_previous_charfun) { g_previous_charfun(window, c); }
        for (auto * cb : keyboards) { cb->charAction(window, c); }
    }
}

static float MoPI = 355/113.0f;
static constexpr MoFloat3 qxdir(const MoFloat4 & q) { return {q.w*q.w+q.x*q.x-q.y*q.y-q.z*q.z, (q.x*q.y+q.z*q.w)*2, (q.z*q.x-q.y*q.w)*2}; }
static constexpr MoFloat3 qydir(const MoFloat4 & q) { return {(q.x*q.y-q.z*q.w)*2, q.w*q.w-q.x*q.x+q.y*q.y-q.z*q.z, (q.y*q.z+q.x*q.w)*2}; }
static constexpr MoFloat3 qzdir(const MoFloat4 & q) { return {(q.z*q.x+q.y*q.w)*2, (q.y*q.z-q.x*q.w)*2, q.w*q.w-q.x*q.x-q.y*q.y+q.z*q.z}; }
static constexpr MoFloat4 rotation_quat(const MoFloat3 & axis, float angle)
{
    const auto a = std::sin(angle/2);
    return {axis.x*a,axis.y*a,axis.z*a,std::cos(angle/2)};
}
static constexpr MoFloat4x4 rotation_matrix(const MoFloat4 & rotation)
{
    const auto a = qxdir(rotation);
    const auto b = qydir(rotation);
    const auto c = qzdir(rotation);
    return {a.x,a.y,a.z,0.f,
            b.x,b.y,b.z,0.f,
            c.x,c.y,c.z,0.f,
            0.f,0.f,0.f,1.f};
}

struct MoMouselook_T
    : public GlfwCallbacks::IMouse
{
    virtual void cursorPositionAction(void *, double xpos, double ypos) override
    {
        if (once)
        {
            const float deltaX = (float)xpos - previousX;
            const float deltaY = (float)ypos - previousY;

            yaw += deltaX * scale * MoPI;
            pitch += deltaY * scale * MoPI/2;

            *azimuth = rotation_matrix(rotation_quat(MoFloat3{0.f,-1.f,0.f}, float(yaw)));
            *altitude = rotation_matrix(rotation_quat(MoFloat3{-1.f,0.f,0.f}, float(pitch)));
        }

        previousX = xpos;
        previousY = ypos;
        once = true;
    }

    virtual void mouseLost() override { once = false; }

    MoFloat4x4 *altitude;
    MoFloat4x4 *azimuth;
    float previousX, previousY, yaw, pitch, scale;
    bool once;
};

void moControlInit(MoControlInitInfo *pInfo)
{
    assert(g_window == nullptr && "Control system cannot be initialized twice.");
    g_window = pInfo->pWindow;
    g_previous_cursorenterfun = glfwSetCursorEnterCallback(g_window, GlfwCallbacks::cursorEnterCallback);
    g_previous_cursorposfun   = glfwSetCursorPosCallback(g_window, GlfwCallbacks::cursorPositionCallback);
    g_previous_mousebuttonfun = glfwSetMouseButtonCallback(g_window, GlfwCallbacks::mouseButtonCallback);
    g_previous_scrollfun      = glfwSetScrollCallback(g_window, GlfwCallbacks::scrollCallback);
    g_previous_keyfun         = glfwSetKeyCallback(g_window, GlfwCallbacks::keyCallback);
    g_previous_charfun        = glfwSetCharCallback(g_window, GlfwCallbacks::charCallback);
}

void moControlShutdown()
{
    glfwSetCursorEnterCallback(g_window, g_previous_cursorenterfun);
    glfwSetCursorPosCallback  (g_window, g_previous_cursorposfun  );
    glfwSetMouseButtonCallback(g_window, g_previous_mousebuttonfun);
    glfwSetScrollCallback     (g_window, g_previous_scrollfun     );
    glfwSetKeyCallback        (g_window, g_previous_keyfun        );
    glfwSetCharCallback       (g_window, g_previous_charfun       );
    g_window = nullptr;
    g_previous_cursorenterfun = nullptr;
    g_previous_cursorposfun   = nullptr;
    g_previous_mousebuttonfun = nullptr;
    g_previous_scrollfun      = nullptr;
    g_previous_keyfun         = nullptr;
    g_previous_charfun        = nullptr;
}

void moCreateMouselook(const MoMouselookCreateInfo *pCreateInfo, MoMouselook *pMouselook)
{
    assert(pCreateInfo->pAltitude != nullptr && "Altitude matrix cannot be null.");
    assert(pCreateInfo->pAzimuth != nullptr && "Azimuth matrix cannot be null.");

    MoMouselook mouselook = *pMouselook = new MoMouselook_T();
    *mouselook = {};
    mouselook->altitude = pCreateInfo->pAltitude;
    mouselook->azimuth = pCreateInfo->pAzimuth;
    mouselook->scale = pCreateInfo->scale;

    GlfwCallbacks::mice.push_back(mouselook);
}

void moDestroyMouselook(MoMouselook mouselook)
{
    GlfwCallbacks::mice.erase(std::remove(GlfwCallbacks::mice.begin(), GlfwCallbacks::mice.end(), mouselook));

    delete mouselook;
}

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
