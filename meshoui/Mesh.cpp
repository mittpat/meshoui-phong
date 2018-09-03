#include "Mesh.h"
#include "GraphicsPrivate.h"
#include "GraphicsUniform.h"

#include "assert.h"
#include <linalg.h>

#include <algorithm>

using namespace linalg;
using namespace linalg::aliases;

Mesh::~Mesh()
{
    for (auto uniform : uniforms)
    {
        delete uniform;
    }
}

void Mesh::add(IGraphicsUniform * uniform)
{
    if (std::find(uniforms.begin(), uniforms.end(), uniform) == uniforms.end())
    {
        uniforms.push_back(uniform);
    }
}

void Mesh::remove(IGraphicsUniform * uniform)
{
    uniforms.erase(std::remove(uniforms.begin(), uniforms.end(), uniform));
}

void Mesh::applyUniforms()
{
    d_ptr()->setProgramUniforms(this);
}

void Mesh::unapplyUniforms()
{
    for (auto* uniform : uniforms)
    {
        d_ptr()->unsetProgramUniform(program, uniform);
    }
}

IGraphicsUniform *Mesh::uniform(HashId name) const
{
    auto found = std::find_if(uniforms.begin(), uniforms.end(), [name](IGraphicsUniform * uniform) { return uniform->name == name; });
    if (found != uniforms.end())
        return *found;
    return nullptr;
}

linalg::aliases::float4x4 Mesh::modelMatrix() const
{
    return mul(translation_matrix(position), mul(rotation_matrix(orientation), scaling_matrix(scale)));
}
