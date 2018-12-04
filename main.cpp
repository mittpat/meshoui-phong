#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <Assets.h>
#include <Camera.h>
#include <Manipulators.h>
#include <Mesh.h>
#include <Program.h>
#include <Renderer.h>

#include <q3.h>

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

namespace
{
    const float timestep = 1.f/60;
}

int main(int, char**)
{
    q3Scene scene(timestep);
    Renderer renderer;

    Program phongProgram;
    phongProgram.load("meshoui/resources/shaders/Phong.shader");
    renderer.add(&phongProgram);

    {
        static const float3 right(-1.,0.,0.);

        ScopedSkydome skydome(&renderer);
        ScopedAsset crates(&renderer, "meshoui/resources/models/crates.dae");
        std::vector<ScopedBody> bodies; bodies.reserve(crates.meshes.size());
        std::vector<BodyAttitude<Mesh>> bodyBakers; bodyBakers.reserve(crates.meshes.size());
        for (const auto & mesh : crates.meshes)
        {
            bodies.emplace_back(&scene, mesh->position, mesh->orientation, mesh->scale);
            bodyBakers.emplace_back(mesh, bodies.back().body);
        }
        ScopedBody groundBody(&scene, float3(0, -5, 0), identity, float3(100, 1, 100), false);


        Camera camera;
        camera.position = float3(0.0, 2.0, 5.0);
        renderer.add(&camera);
        camera.enable();

        LinearAcceleration<Camera> cameraAnimator(&camera, 0.1f);
        WASD<LinearAcceleration<Camera>> cameraStrafer(&cameraAnimator);
        renderer.add(&cameraStrafer);

        Mouselook cameraLook(&camera);
        renderer.add(&cameraLook);


        Light light;
        light.position = float3(300.0, 1000.0, -300.0);
        renderer.add(&light);
        light.enable(true);

        AngularVelocity<Light> lightAnimator(&light);
        lightAnimator.angularVelocity = rotation_quat(right, 0.02f);


        bool run = true;
        while (run)
        {
            lightAnimator.step(timestep);
            cameraAnimator.step(timestep);
            scene.Step();
            for (const auto & bodyBaker : bodyBakers)
                bodyBaker.bake();
            renderer.update(timestep);
            if (renderer.shouldClose())
                run = false;
        }

        renderer.remove(&cameraStrafer);
        renderer.remove(&light);
        renderer.remove(&camera);
    }

    renderer.remove(&phongProgram);

    return 0;
}
