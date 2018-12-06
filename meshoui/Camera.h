#pragma once

#include "enums.h"
#include <hashid.h>
#include <linalg.h>

namespace Meshoui
{
    class RendererPrivate;
    class Camera
    {
    public:
        virtual ~Camera();
        Camera();
        void enable(bool asLight = false);
        void disable();
        /*linalg::aliases::float4x4 modelMatrix() const;*/
        linalg::aliases::float4x4 viewMatrix(View::Flags op = View::All) const;

        HashId name;
        linalg::aliases::float4 orientation;
        linalg::aliases::float3 position;

    private:
        friend class RendererPrivate;
        RendererPrivate * d;
    };
    inline Camera::Camera() : orientation(linalg::identity), position(0.f, 0.f, 0.f) {}
    inline Camera::~Camera() {}

    typedef Camera Light;
}
