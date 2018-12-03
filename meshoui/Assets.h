#pragma once

#include "Program.h"
#include "Renderer.h"

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
}
