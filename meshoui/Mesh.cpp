#include "Mesh.h"
#include "RendererPrivate.h"

#include <linalg.h>

#include <algorithm>

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

size_t Model::meshCount() const
{
    return d->load(filename).instances.size();
}

void Model::fill(const std::vector<Mesh *> & meshes) const
{
    d->fill(filename, meshes);
}

Mesh::~Mesh()
{

}

linalg::aliases::float4x4 Mesh::modelMatrix() const
{
    return mul(translation_matrix(position), mul(rotation_matrix(orientation), scaling_matrix(scale)));
}
