#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <Assets.h>
#include <Camera.h>
#include <Manipulators.h>
#include <Mesh.h>
#include <Program.h>
#include <Renderer.h>

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

int main(int, char**)
{
    Renderer renderer;

    Program phongProgram;
    phongProgram.load("meshoui/resources/shaders/Phong.shader");
    renderer.add(&phongProgram);

    {
        static const float3 up(0.,1.,0.);
        static const float3 right(-1.,0.,0.);

        ScopedSkydome skydome(&renderer);
        ScopedAsset crates(&renderer, "meshoui/resources/models/crates.dae");

        Camera camera;
        camera.position = float3(0.0, 2.0, 5.0);
        renderer.add(&camera);
        camera.enable();

        Light light;
        light.position = mul(rotation_matrix(qmul(rotation_quat(up, -0.4f), rotation_quat(right, 0.0f))), float4(0., 0., 1., 1.0)).xyz() * 1000.0f;
        renderer.add(&light);
        light.enable(true);

        LinearVelocity<Camera> cameraAnimator(&camera, 0.1f);
        WASD<LinearVelocity<Camera>> cameraStrafer(&cameraAnimator);
        renderer.add(&cameraStrafer);

        Mouselook cameraLook(&camera);
        renderer.add(&cameraLook);

        bool run = true;
        while (run)
        {
            light.position = mul(rotation_matrix(qmul(rotation_quat(up, -0.4f), rotation_quat(right, 0.0f + renderer.time * 0.1f))), float4(0., 0., 1., 1.0)).xyz() * 1000.0f;
            cameraAnimator.step(0.016f);
            renderer.update(0.016f);
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
