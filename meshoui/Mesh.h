#pragma once

#include "IGraphics.h"
#include "IWhatever.h"

#include "enums.h"
#include "hashid.h"
#include <linalg.h>

#include <string>
#include <vector>

class GraphicsProgram;
class IGraphicsUniform;
class Mesh
    : public IWhatever
    , public IGraphics
{
public:
    virtual ~Mesh();
    Mesh();

    void add(IGraphicsUniform * uniform);
    void remove(IGraphicsUniform * uniform);
    void applyUniforms();
    void unapplyUniforms();
    IGraphicsUniform * uniform(HashId name) const;
    linalg::aliases::float4x4 modelMatrix() const;

    HashId name;
    HashId instanceId;
    HashId definitionId;
    std::string filename;
    GraphicsProgram * program;

    std::vector<IGraphicsUniform *> uniforms;

    linalg::aliases::float3 scale;
    linalg::aliases::float4 orientation;
    linalg::aliases::float3 position;

    View::Flags viewFlags;
    Render::Flags renderFlags;
};
inline Mesh::Mesh() : program(nullptr), scale(1.f, 1.f, 1.f), orientation(linalg::rotation_quat(linalg::aliases::float3(0.f, 1.f, 0.f), 0.f)), position(0.f, 0.f, 0.f), viewFlags(View::All), renderFlags(Render::Default) {}
