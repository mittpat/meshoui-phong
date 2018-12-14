#include "Camera.h"
#include "RendererPrivate.h"

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

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
    float4x4 ret = inverse(modelMatrix);
    if ((op & View::Translation) == 0) { ret.w = float4(0.f, 0.f, 0.f, 1.f); }
    if ((op & View::Rotation) == 0)
    {
        ret.x = float4(1.f, 0.f, 0.f, 0.f);
        ret.y = float4(0.f, 1.f, 0.f, 0.f);
        ret.z = float4(0.f, 0.f, 1.f, 0.f);
    }
    return ret;
}
