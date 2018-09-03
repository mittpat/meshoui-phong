#pragma once

#include "enums.h"
#include <linalg.h>

class Camera final
{
public:
    virtual ~Camera();
    Camera();

    linalg::aliases::float4x4 viewMatrix(View::Flags op = View::All) const;

    linalg::aliases::float4 orientation;
    linalg::aliases::float3 position;
};
inline Camera::Camera() : orientation(linalg::rotation_quat(linalg::aliases::float3(0.f, 1.f, 0.f), 0.f)), position(0.f, 0.f, 0.f) {}
inline Camera::~Camera() {}
