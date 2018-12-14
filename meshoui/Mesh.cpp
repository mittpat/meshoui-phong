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
