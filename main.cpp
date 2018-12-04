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
        static const float3 right(-1.,0.,0.);

        ScopedSkydome skydome(&renderer);
        ScopedAsset crates(&renderer, "meshoui/resources/models/crates.dae");


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
            lightAnimator.step(0.016f);
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
