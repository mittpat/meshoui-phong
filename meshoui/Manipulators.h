#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <linalg.h>

#include "IKeyboard.h"
#include "IMouse.h"

namespace Meshoui
{
    template<typename T>
    struct LinearVelocity
    {
        LinearVelocity(T * t, float d = 0.f) : target(t), damping(d), linearVelocity(linalg::zero), linearAcceleration(linalg::zero) {}
        void step(float s)
        {
            linalg::aliases::float3 forward = qzdir(target->orientation);
            linalg::aliases::float3 right = qxdir(target->orientation);

            linearVelocity *= (1.0f - damping);
            linearVelocity += s * linearAcceleration;
            target->position += s * (linearVelocity.z * forward) + s * (linearVelocity.x * right);
        }
        T * target;
        float damping;
        linalg::aliases::float3 linearVelocity;
        linalg::aliases::float3 linearAcceleration;
    };

    template<typename T>
    struct WASD
        : IKeyboard
    {
        WASD(T * t) : target(t) {}
        virtual void keyAction(void *, int key, int, int action, int) override
        {
            if (action == GLFW_PRESS)
            {
                switch (key)
                {
                case GLFW_KEY_W:
                    target->position.z -= 0.1;
                    break;
                case GLFW_KEY_A:
                    target->position.x -= 0.1;
                    break;
                case GLFW_KEY_S:
                    target->position.z += 0.1;
                    break;
                case GLFW_KEY_D:
                    target->position.x += 0.1;
                    break;
                default:
                    break;
                }
            }
        }
        T * target;
    };

    template<typename T>
    struct WASD<LinearVelocity<T>>
        : IKeyboard
    {
        WASD(LinearVelocity<T> * t) : target(t), w(false), a(false), s(false), d(false) {}
        virtual void keyAction(void *, int key, int, int action, int) override
        {
            static const float v = 100.f;
            if (action == GLFW_PRESS)
            {
                switch (key)
                {
                case GLFW_KEY_W:
                    if (target->linearAcceleration.z > 0.f)
                        target->linearAcceleration.z = 0.f;
                    target->linearAcceleration.z -= v;
                    w = true;
                    break;
                case GLFW_KEY_A:
                    if (target->linearAcceleration.x > 0.f)
                        target->linearAcceleration.x = 0.f;
                    target->linearAcceleration.x -= v;
                    a = true;
                    break;
                case GLFW_KEY_S:
                    if (target->linearAcceleration.z < 0.f)
                        target->linearAcceleration.z = 0.f;
                    target->linearAcceleration.z += v;
                    s = true;
                    break;
                case GLFW_KEY_D:
                    if (target->linearAcceleration.x < 0.f)
                        target->linearAcceleration.x = 0.f;
                    target->linearAcceleration.x += v;
                    d = true;
                    break;
                default:
                    break;
                }
            }
            if (action == GLFW_RELEASE)
            {
                switch (key)
                {
                case GLFW_KEY_W:
                    target->linearAcceleration.z = 0.0;
                    w = false;
                    if (s) target->linearAcceleration.z += v;
                    break;
                case GLFW_KEY_A:
                    target->linearAcceleration.x = 0.0;
                    a = false;
                    if (d) target->linearAcceleration.x += v;
                    break;
                case GLFW_KEY_S:
                    target->linearAcceleration.z = 0.0;
                    s = false;
                    if (w) target->linearAcceleration.z -= v;
                    break;
                case GLFW_KEY_D:
                    target->linearAcceleration.x = 0.0;
                    d = false;
                    if (a) target->linearAcceleration.x -= v;
                    break;
                default:
                    break;
                }
            }
        }
        LinearVelocity<T> * target;
        bool w,a,s,d;
    };

    template<typename T>
    struct Mouselook
        : IMouse
    {
        Mouselook(T * t) : target(t), previousX(0), previousY(0), once(false) {}
        virtual void cursorPositionAction(void *, double xpos, double ypos, bool overlay) override
        {
            if (overlay)
                return;

            if (once)
            {
                double deltaX = xpos - previousX;
                double deltaY = ypos - previousY;

                static const float rotationScaler = 0.0005f;
                {
                    linalg::aliases::float3 right = qxdir(target->orientation);
                    target->orientation = qmul(rotation_quat(right, float(-deltaY * rotationScaler * M_PI/2)), target->orientation);
                }
                {
                    linalg::aliases::float3 up(0.,1.,0.);
                    target->orientation = qmul(rotation_quat(up, float(-deltaX * rotationScaler * M_PI)), target->orientation);
                }
            }

            previousX = xpos;
            previousY = ypos;
            once = true;
        }
        virtual void mouseLost() override
        {
            once = false;
        }
        T * target;
        double previousX, previousY;
        bool once;
    };
}
