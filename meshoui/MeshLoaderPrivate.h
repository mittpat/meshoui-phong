#pragma once

namespace DAE
{
    struct Mesh;
}

namespace Meshoui
{
    class MeshDefinition;
    namespace MeshLoader
    {
        void buildGeometry(MeshDefinition & definition, const DAE::Mesh & mesh, bool renormalize = false);
    }
}
