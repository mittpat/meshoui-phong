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
        ScopedBody(q3Scene * s, const linalg::aliases::float3 & position = linalg::zero, const linalg::aliases::float4 & orientation = linalg::identity, const linalg::aliases::float3 & scale = linalg::aliases::float3(1.f, 1.f, 1.f), bool dynamic = true);
        ~ScopedBody();

        q3Scene * scene;
        q3Body * body;
    };
}
