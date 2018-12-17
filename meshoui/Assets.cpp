#include "Assets.h"

#include <q3.h>

#include <fstream>

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
{
    {
        std::ifstream fileStream("meshoui/resources/shaders/Skydome.vert.spv", std::ifstream::binary);
        program.vertexShaderSource = std::vector<char>(std::istreambuf_iterator<char>(fileStream), std::istreambuf_iterator<char>());
    }
    {
        std::ifstream fileStream("meshoui/resources/shaders/Skydome.frag.spv", std::ifstream::binary);
        program.fragmentShaderSource = std::vector<char>(std::istreambuf_iterator<char>(fileStream), std::istreambuf_iterator<char>());
    }

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

ScopedBody::ScopedBody(q3Scene *s, const linalg::aliases::float4x4 & initial,
                       linalg::aliases::float4x4 * t, bool dynamic, bool upright, float sc)
    : scene(s)
    , body(nullptr)
    , target(t)
{
    q3BodyDef def;
    def.bodyType = dynamic ? eDynamicBody : eKinematicBody;
    if (upright)
    {
        def.lockAxisX = def.lockAxisZ = true;
    }
    def.allowSleep = false;
    def.awake = true;
    def.active = true;
    body = scene->CreateBody(def);

    q3BoxDef boxDef;
    boxDef.SetRestitution(0);
    q3Transform tx;
    q3Identity(tx);

    float3 position;
    float3x3 rotation;
    float3 scale;
    linalg::split(initial, position, scale, rotation);
    float4 orientation = rotation_quat(rotation);

    boxDef.Set(tx, (q3Vec3&)scale * 2.f * sc);
    body->AddBox(boxDef);
    float3 axis = qaxis(orientation);
    body->SetTransform((q3Vec3&)position, (q3Vec3&)axis, qangle(orientation));
}

ScopedBody::~ScopedBody()
{
    scene->RemoveBody(body);
}

void ScopedBody::step(float)
{
    if (target)
    {
        const auto transform = body->GetTransform();
        *target = pose_matrix(rotation_quat((float3x3&)transform.rotation), (float3&)transform.position);
    }
}

void ScopedBody::setAzimuth(const float4x4 &m)
{
    float4 forward(0,0,1,0);
    forward = mul(m, forward);
    forward.y = 0;
    forward = normalize(forward);

    float4 other(0,0,1,0);
    //other = mul(*target, other);

    float a = angle(forward.xyz(), other.xyz());
    auto c = cross(forward.xyz(), other.xyz());
    if (dot(float3(0,1,0), c) < 0) { a = -a; }
    body->SetTransform(body->GetTransform().position, q3Vec3(0,-1,0), a);

    //printf("%f\n", a);
    //fflush(stdout);
    //
    //body->ApplyTorque(q3Vec3(0,(a / 3.1416f)*(a / 3.1416f)*(a / 3.1416f)*0.1,0));
}

void ScopedBody::jump(bool j)
{
    static bool p = false;
    if (j && !p)
        body->ApplyLinearImpulse(q3Vec3(0,75/100.0,0));
    p = j;
}
