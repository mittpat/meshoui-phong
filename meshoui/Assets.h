#pragma once

#include "Program.h"
#include "Renderer.h"

class q3Body;
class q3Scene;
namespace Meshoui
{
    struct ScopedAsset
    {
        ScopedAsset(Renderer * r, const std::string & filename);
        virtual ~ScopedAsset();

        Renderer * renderer;
        Model model;
        std::vector<Mesh*> meshes;
    };

    struct ScopedSkydome final
        : ScopedAsset
    {
        ScopedSkydome(Renderer * r);
        virtual ~ScopedSkydome();

        Program program;
    };

    struct ScopedBody final
    {
        ScopedBody(q3Scene * s, const linalg::aliases::float4x4 & initial = linalg::identity,
                   linalg::aliases::float4x4 * t = nullptr, bool dynamic = true, bool upright = false, float sc = 1.0f);
        ~ScopedBody();
        void step(float);
        void setAzimuth(const linalg::aliases::float4x4 & m);
        void jump(bool j);

        q3Scene * scene;
        q3Body * body;
        linalg::aliases::float4x4 * target;
    };
}
