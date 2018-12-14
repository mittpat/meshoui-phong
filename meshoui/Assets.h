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
        ScopedBody(q3Scene * s,
                   const linalg::aliases::float4x4 & modelMatrix = linalg::identity,
                   bool dynamic = true, bool upright = false);
        ~ScopedBody();

        linalg::aliases::float4x4 modelMatrix() const;

        q3Scene * scene;
        q3Body * body;
    };
}
