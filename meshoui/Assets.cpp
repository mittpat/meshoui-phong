#include "Assets.h"

#include <q3.h>

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

ScopedBody::ScopedBody(q3Scene *s, const linalg::aliases::float3 & position, const linalg::aliases::float4 & orientation, const linalg::aliases::float3 &scale, bool dynamic)
    : scene(s)
    , body(nullptr)
{
    q3BodyDef def;
    def.bodyType = dynamic ? eDynamicBody : eKinematicBody;
    body = scene->CreateBody(def);

    q3BoxDef boxDef;
    boxDef.SetRestitution(0);
    q3Transform tx;
    q3Identity(tx);

    boxDef.Set(tx, (q3Vec3&)scale * 2.f);
    body->AddBox(boxDef);
    linalg::aliases::float3 axis = linalg::qaxis(orientation);
    body->SetTransform((q3Vec3&)position, (q3Vec3&)axis, linalg::qangle(orientation));
}

ScopedBody::~ScopedBody()
{
    scene->RemoveBody(body);
}
