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
        linalg::aliases::float4x4 viewMatrix(View::Flags op = View::All) const;

        HashId name;
        linalg::aliases::float4x4 modelMatrix;

    private:
        friend class RendererPrivate;
        RendererPrivate * d;
    };
    inline Camera::Camera() : modelMatrix(linalg::identity) {}
    inline Camera::~Camera() {}

    typedef Camera Light;
}
