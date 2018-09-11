#include "Camera.h"
#include "RendererPrivate.h"

using namespace linalg;
using namespace linalg::aliases;

void Camera::enable(bool asLight)
{
    d->bindGraphics(this, asLight);
}

void Camera::disable()
{
    d->unbindGraphics(this);
}

float4x4 Camera::modelMatrix() const
{
    return mul(translation_matrix(position), mul(rotation_matrix(orientation), scaling_matrix(float3(1.0, 1.0, 1.0))));
}

linalg::aliases::float4x4 Camera::viewMatrix(View::Flags op) const
{
    linalg::aliases::float4x4 ret = identity;
    if (op & View::Scaling)
        ret = mul(scaling_matrix(float3(1.0, 1.0, 1.0)), ret);
    if (op & View::Rotation)
        ret = mul(rotation_matrix(orientation), ret);
    if (op & View::Translation)
        ret = mul(translation_matrix(position), ret);
    return inverse(ret);
}
