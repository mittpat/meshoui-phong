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

struct MoMouselook_T
    : GlfwCallbacks::IMouse
{
    virtual void cursorPositionAction(void *, double xpos, double ypos) override
    {
        if (once)
        {
            *yaw += (float)xpos - previousX;
            *pitch += (float)ypos - previousY;
        }
        previousX = xpos;
        previousY = ypos;
        once = true;
    }
    virtual void mouseLost() override { once = false; }

    float *yaw, *pitch, previousX, previousY;
    bool once;
};

struct MoStrafer_T
    : GlfwCallbacks::IKeyboard
{
    virtual void keyAction(void *, int key, int, int action, int) override
    {
        if (action == GLFW_PRESS)
        {
            if (key == keyW) *w = true;
            if (key == keyA) *a = true;
            if (key == keyS) *s = true;
            if (key == keyD) *d = true;
        }
        if (action == GLFW_RELEASE)
        {
            if (key == keyW) *w = false;
            if (key == keyA) *a = false;
            if (key == keyS) *s = false;
            if (key == keyD) *d = false;
        }
    }
    bool *w,*a,*s,*d;
    int keyW,keyA,keyS,keyD;
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
    assert(pCreateInfo->pYaw != nullptr && "Yaw pointer cannot be null.");
    assert(pCreateInfo->pPitch != nullptr && "Pitch pointer cannot be null.");

    MoMouselook mouselook = *pMouselook = new MoMouselook_T();
    *mouselook = {};
    mouselook->pitch = pCreateInfo->pPitch;
    mouselook->yaw = pCreateInfo->pYaw;
    GlfwCallbacks::mice.push_back(mouselook);
}

void moResetMouselook(MoMouselook mouselook)
{
    mouselook->previousX = mouselook->previousY = *mouselook->yaw = *mouselook->pitch = 0.f;
    mouselook->once = false;
}

void moDestroyMouselook(MoMouselook mouselook)
{
    GlfwCallbacks::mice.erase(std::remove(GlfwCallbacks::mice.begin(), GlfwCallbacks::mice.end(), mouselook));
    delete mouselook;
}

void moCreateStrafer(const MoStraferCreateInfo* pCreateInfo, MoStrafer* pStrafer)
{
    assert(pCreateInfo->pForward != nullptr && "w cannot be null.");
    assert(pCreateInfo->pLeft != nullptr && "a cannot be null.");
    assert(pCreateInfo->pBackward != nullptr && "s cannot be null.");
    assert(pCreateInfo->pRight != nullptr && "d cannot be null.");

    MoStrafer strafer = *pStrafer = new MoStrafer_T();
    *strafer = {};
    strafer->w = pCreateInfo->pForward;
    strafer->a = pCreateInfo->pLeft;
    strafer->s = pCreateInfo->pBackward;
    strafer->d = pCreateInfo->pRight;
    strafer->keyW = pCreateInfo->keyForward;
    strafer->keyA = pCreateInfo->keyLeft;
    strafer->keyS = pCreateInfo->keyBackward;
    strafer->keyD = pCreateInfo->keyRight;
    if (strafer->keyW == 0) strafer->keyW = GLFW_KEY_W;
    if (strafer->keyA == 0) strafer->keyW = GLFW_KEY_A;
    if (strafer->keyS == 0) strafer->keyW = GLFW_KEY_S;
    if (strafer->keyD == 0) strafer->keyW = GLFW_KEY_D;
    GlfwCallbacks::keyboards.push_back(strafer);
}

void moDestroyStrafer(MoStrafer strafer)
{
    GlfwCallbacks::keyboards.erase(std::remove(GlfwCallbacks::keyboards.begin(), GlfwCallbacks::keyboards.end(), strafer));
    delete strafer;
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
