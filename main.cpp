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
        ScopedSkydome skydome(&renderer);
        ScopedAsset bricks(&renderer, "meshoui/resources/models/bricks.dae");
        ScopedAsset island(&renderer, "meshoui/resources/models/island.dae");
        for (const auto & mesh : island.meshes) mesh->modelMatrix.w.y = -4.0f;
        ScopedAsset crates(&renderer, "meshoui/resources/models/crates.dae");
        std::vector<ScopedBody> bodies; bodies.reserve(crates.meshes.size());
        for (const auto & mesh : crates.meshes) { bodies.emplace_back(&scene, mesh->modelMatrix); }
        bricks.meshes[0]->modelMatrix.w = float4(float3(-5.0, 2.0, 0.0), 1.0);
        float4x4 groundModel = mul(pose_matrix(float4(0,0,0,1), float3(0,-5,0)), scaling_matrix(float3(100,1,100)));
        ScopedBody groundBody(&scene, groundModel, false);
        
        Camera camera;
        camera.modelMatrix.w = float4(float3(0.0, 2.0, 5.0), 1.0);
        renderer.add(&camera);
        camera.enable();

        LinearAcceleration<Camera> cameraAnimator(&camera, 0.1f);
        WASD<LinearAcceleration<Camera>> cameraStrafer(&cameraAnimator);
        renderer.add(&cameraStrafer);

        Mouselook<Camera> cameraLook(&camera);
        renderer.add(&cameraLook);

        Light light;
        light.modelMatrix.w = float4(float3(300.0, 1000.0, -300.0), 1.0);
        renderer.add(&light);
        light.enable(true);
        
        bool run = true;
        while (run)
        {
            cameraAnimator.step(timestep);
            scene.Step();
            for (size_t i = 0; i < crates.meshes.size(); ++i) { crates.meshes[i]->modelMatrix = bodies[i].modelMatrix(); }
            renderer.update(timestep);
            if (renderer.shouldClose()) { run = false; }
        }

        renderer.remove(&cameraStrafer);
        renderer.remove(&light);
        renderer.remove(&camera);
    }

    renderer.remove(&phongProgram);

    return 0;
}
