#include "Mesh.h"
#include "RendererPrivate.h"
#include "Uniform.h"

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

void Mesh::add(IUniform * uniform)
{
    if (std::find(uniforms.begin(), uniforms.end(), uniform) == uniforms.end())
    {
        uniforms.push_back(uniform);
    }
}

void Mesh::remove(IUniform * uniform)
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

IUniform *Mesh::uniform(HashId name) const
{
    auto found = std::find_if(uniforms.begin(), uniforms.end(), [name](IUniform * uniform) { return uniform->name == name; });
    if (found != uniforms.end())
        return *found;
    return nullptr;
}

linalg::aliases::float4x4 Mesh::modelMatrix() const
{
    return mul(translation_matrix(position), mul(rotation_matrix(orientation), scaling_matrix(scale)));
}
