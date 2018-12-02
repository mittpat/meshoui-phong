#include <Camera.h>
#include <Renderer.h>
#include <Program.h>
#include <Mesh.h>

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

struct Skydome
{
    Skydome(Renderer * r)
        : renderer(r)
        , program("meshoui/resources/shaders/Skydome.shader")
        , model("meshoui/resources/models/skydome.dae")
    {
        program.features &= ~Feature::DepthTest;
        program.features &= ~Feature::DepthWrite;
        renderer->add(&program);
        renderer->add(&model);
        meshes = model.meshFactory<Mesh>();
        for (auto mesh : meshes)
        {
            mesh->viewFlags = Meshoui::View::Rotation;
            mesh->program = &program;
            renderer->add(mesh);
        }
    }

    ~Skydome()
    {
        for (auto mesh : meshes) renderer->remove(mesh);
        renderer->remove(&model);
        renderer->remove(&program);
    }

    Renderer * renderer;
    Program program;
    Model model;
    std::vector<Mesh*> meshes;
};

struct Asset
{
    Asset(Renderer * r, const std::string & filename)
        : renderer(r)
        , model(filename)
    {
        renderer->add(&model);
        meshes = model.meshFactory<Mesh>();
        for (auto mesh : meshes) renderer->add(mesh);
    }

    ~Asset()
    {
        for (auto mesh : meshes) renderer->remove(mesh);
        renderer->remove(&model);
    }

    Renderer * renderer;
    Model model;
    std::vector<Mesh*> meshes;
};

int main(int, char**)
{
    Renderer renderer;

    Program phongProgram;
    phongProgram.load("meshoui/resources/shaders/Phong.shader");
    renderer.add(&phongProgram);

    {
        static const float3 up(0.,1.,0.);
        static const float3 right(-1.,0.,0.);

        Skydome skydome(&renderer);
        Asset crates(&renderer, "meshoui/resources/models/crates.dae");

        Camera camera;
        camera.position = float3(0.0, 2.0, 5.0);
        renderer.add(&camera);
        camera.enable();

        Light light;
        light.position = mul(rotation_matrix(qmul(rotation_quat(up, -0.4f), rotation_quat(right, 0.0f))), float4(0., 0., 1., 1.0)).xyz() * 1000.0f;
        renderer.add(&light);
        light.enable(true);

        bool run = true;
        while (run)
        {
            light.position = mul(rotation_matrix(qmul(rotation_quat(up, -0.4f), rotation_quat(right, 0.0f + renderer.time * 0.1f))), float4(0., 0., 1., 1.0)).xyz() * 1000.0f;
            renderer.update(0.016f);
            if (renderer.shouldClose())
                run = false;
        }

        renderer.remove(&light);
        renderer.remove(&camera);
    }

    renderer.remove(&phongProgram);

    return 0;
}
