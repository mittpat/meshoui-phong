#include "Assets.h"

using namespace Meshoui;

ScopedAsset::ScopedAsset(Renderer *r, const std::string &filename)
    : renderer(r)
    , model(filename)
{
    renderer->add(&model);
    meshes = model.meshFactory<Mesh>();
    for (auto mesh : meshes) renderer->add(mesh);
}

ScopedAsset::~ScopedAsset()
{
    for (auto mesh : meshes) renderer->remove(mesh);
    renderer->remove(&model);
}

ScopedSkydome::ScopedSkydome(Renderer *r)
    : ScopedAsset(r, "meshoui/resources/models/skydome.dae")
    , program("meshoui/resources/shaders/Skydome.shader")
{
    program.features &= ~Feature::DepthTest;
    program.features &= ~Feature::DepthWrite;
    renderer->add(&program);
    for (auto mesh : meshes)
    {
        mesh->viewFlags = Meshoui::View::Rotation;
        mesh->program = &program;
    }
}

ScopedSkydome::~ScopedSkydome()
{
    renderer->remove(&program);
}
