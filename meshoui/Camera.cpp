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

linalg::aliases::float4x4 Camera::viewMatrix(View::Flags op) const
{
    linalg::aliases::float4x4 ret = identity;
    if (op & View::Vertical)
        ret = mul(translation_matrix(float3(0.f, position.y, 0.f)), ret);
    if (op & View::Horizontal)
        ret = mul(translation_matrix(float3(position.x, 0.f, position.z)), ret);
    if (op & View::Rotation)
        ret = mul(rotation_matrix(orientation), ret);
    if (op & View::Scaling)
        ret = mul(scaling_matrix(float3(1.0, 1.0, 1.0)), ret);
    return ret;
}
