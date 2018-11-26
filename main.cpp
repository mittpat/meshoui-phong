#include <Camera.h>
#include <Renderer.h>
#include <Program.h>
#include <Mesh.h>

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

int main(int, char**)
{
    Renderer renderer;

    Program program;
    program.load("meshoui/resources/shaders/Phong.shader");
    renderer.add(&program);

    Camera camera;
    camera.position = float3(0.0, 2.0, 5.0);
    renderer.add(&camera);

    static const float3 up(0.,1.,0.);
    static const float3 right(-1.,0.,0.);

    Light light;
    light.position = mul(rotation_matrix(qmul(rotation_quat(up, 0.8f), rotation_quat(right, 0.6f))), float4(0., 0., 1., 1.0)).xyz() * 1000.0f;
    renderer.add(&light);

    Model model("meshoui/resources/models/bricks.dae");
    renderer.add(&model);

    {
        camera.enable();
        light.enable(true);

        auto meshes = model.meshFactory<Mesh>();
        for (auto mesh : meshes) renderer.add(mesh);

        bool run = true;
        while (run)
        {
            light.position = mul(rotation_matrix(qmul(rotation_quat(up, 0.8f + renderer.time), rotation_quat(right, 0.6f))), float4(0., 0., 1., 1.0)).xyz() * 1000.0f;
            renderer.update(0.016f);
            if (renderer.shouldClose())
                run = false;
        }

        for (auto mesh : meshes) renderer.remove(mesh);
    }

    renderer.remove(&model);
    renderer.remove(&light);
    renderer.remove(&camera);
    renderer.remove(&program);

    return 0;
}
