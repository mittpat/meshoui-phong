#include <Camera.h>
#include <Renderer.h>
#include <Program.h>
#include <Uniform.h>
#include <Mesh.h>

#include <SDL2/SDL.h>

using namespace linalg;
using namespace linalg::aliases;

int main(int, char**)
{
    Renderer renderer;

    Program program;
    program.load("meshoui/resources/shaders/Phong.shader");
    renderer.add(&program);

    Camera camera;
    camera.position = linalg::aliases::float3(0.0, -2.0, -5.0f);
    renderer.add(&camera);

    static const float3 up(0.,1.,0.);
    static const float3 right(-1.,0.,0.);

    Light light;
    light.position = mul(rotation_matrix(qmul(rotation_quat(up, 0.8f), rotation_quat(right, 0.6f))), float4(0., 0., 1., 1.0)).xyz() * 1000.0f;
    renderer.add(&light);

    {
        camera.enable();
        light.enable(true);

        auto meshes = renderer.meshFactory<Mesh>("meshoui/resources/models/bricks.dae");

        bool run = true;
        while (run)
        {
            SDL_PumpEvents();
            SDL_Event * event = nullptr;
            if (0 != SDL_PeepEvents(event, 8, SDL_GETEVENT, SDL_QUIT, SDL_QUIT))
                run = false;

            renderer.update(0.016);
        }

        for (auto mesh : meshes) renderer.remove(mesh);
    }

    renderer.remove(&light);
    renderer.remove(&camera);
    renderer.remove(&program);

    return 0;
}
