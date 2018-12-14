#include "Assets.h"

#include <q3.h>

using namespace Meshoui;
using namespace linalg;
using namespace linalg::aliases;

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

ScopedBody::ScopedBody(q3Scene *s, const float4x4 & modelMatrix, bool dynamic, bool upright)
    : scene(s)
    , body(nullptr)
{
    q3BodyDef def;
    def.bodyType = dynamic ? eDynamicBody : eKinematicBody;
    if (upright)
    {
        def.lockAxisX = def.lockAxisZ = true;
    }
    body = scene->CreateBody(def);

    q3BoxDef boxDef;
    boxDef.SetRestitution(0);
    q3Transform tx;
    q3Identity(tx);

    float3 position;
    float3x3 rotation;
    float3 scale;
    linalg::split(modelMatrix, position, scale, rotation);
    float4 orientation = rotation_quat(rotation);

    boxDef.Set(tx, (q3Vec3&)scale * 2.f);
    body->AddBox(boxDef);
    float3 axis = qaxis(orientation);
    body->SetTransform((q3Vec3&)position, (q3Vec3&)axis, qangle(orientation));
}

ScopedBody::~ScopedBody()
{
    scene->RemoveBody(body);
}

float4x4 ScopedBody::modelMatrix() const
{
    const auto transform = body->GetTransform();
    return pose_matrix(rotation_quat((float3x3&)transform.rotation), (float3&)transform.position);
}
