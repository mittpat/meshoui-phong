#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <linalg.h>
#include <dynamics/q3Body.h>

#include <IKeyboard.h>
#include <IMouse.h>

#define _USE_MATH_DEFINES
#include <math.h>

struct LinearAcceleration final
{
    LinearAcceleration(linalg::aliases::float4x4 * t, float d = 0.f) : target(t), damping(d), linearVelocity(linalg::zero), linearAcceleration(linalg::zero) {}
    void step(float s)
    {
        linalg::aliases::float3 forward = mul(*target, linalg::aliases::float4(0,0,1,0)).xyz();
        linalg::aliases::float3 right = mul(*target, linalg::aliases::float4(1,0,0,0)).xyz();

        linearVelocity *= (1.0f - damping);
        linearVelocity += s * linearAcceleration;
        target->w += linalg::aliases::float4(s * (linearVelocity.z * forward) + s * (linearVelocity.x * right), 0);
    }
    linalg::aliases::float4x4 * target;
    float damping;
    linalg::aliases::float3 linearVelocity;
    linalg::aliases::float3 linearAcceleration;
};

struct BodyAcceleration final
{
    BodyAcceleration(q3Body * b, float d = 0.f) : body(b), damping(d), linearAcceleration(linalg::zero) {}
    void step(float s)
    {
        const auto transform = body->GetTransform();
        const auto target = pose_matrix(rotation_quat((linalg::aliases::float3x3&)transform.rotation), (linalg::aliases::float3&)transform.position);

        linalg::aliases::float3 forward = mul(target, linalg::aliases::float4(0,0,1,0)).xyz();
        linalg::aliases::float3 right = mul(target, linalg::aliases::float4(1,0,0,0)).xyz();

        linalg::aliases::float3 linearVelocity;
        (q3Vec3&)linearVelocity = body->GetLinearVelocity();
        linearVelocity.x *= (1.0f - damping);
        linearVelocity.z *= (1.0f - damping);
        linearVelocity += s * (linearAcceleration.z * forward) + s * (linearAcceleration.x * right);
        body->SetLinearVelocity((q3Vec3&)linearVelocity);
    }
    q3Body * body;
    float damping;
    linalg::aliases::float3 linearAcceleration;
};

template <typename T>
struct WASD final
    : Meshoui::IKeyboard
{
    WASD(T * t) : target(t), w(false), a(false), s(false), d(false), space(false) {}
    virtual void keyAction(void *, int key, int, int action, int) override
    {
        static const float v = 75.f;
        if (action == GLFW_PRESS)
        {
            switch (key)
            {
            case GLFW_KEY_SPACE:
                space = true;
                break;
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
            case GLFW_KEY_SPACE:
                space = false;
                break;
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
    T * target;
    bool w,a,s,d,space;
};

struct Mouselook final
    : Meshoui::IMouse
{
    Mouselook(linalg::aliases::float4x4 * al, linalg::aliases::float4x4 * az) : altitude(al), azimuth(az), previousX(0), previousY(0), yaw(0), pitch(0), once(false) {}
    virtual void cursorPositionAction(void *, double xpos, double ypos, bool overlay) override
    {
        if (overlay)
            return;

        if (once)
        {
            double deltaX = xpos - previousX;
            double deltaY = ypos - previousY;

            static const float rotationScaler = 0.001f;
            {
                //linalg::aliases::float3 position = target->w.xyz();

                yaw += deltaX * rotationScaler * M_PI;
                pitch += deltaY * rotationScaler * M_PI/2;

                *azimuth = rotation_matrix(rotation_quat(linalg::aliases::float3(0,-1,0), float(yaw)));
                *altitude = rotation_matrix(rotation_quat(linalg::aliases::float3(-1,0,0), float(pitch)));

                //*target = mul(mul(translation_matrix(position), yawM), pitchM);
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
    linalg::aliases::float4x4 * altitude;
    linalg::aliases::float4x4 * azimuth;
    double previousX, previousY, yaw, pitch;
    bool once;
};
