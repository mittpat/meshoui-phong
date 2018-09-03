#include <Camera.h>
#include <Renderer.h>
#include <Program.h>
#include <Uniform.h>
#include <Mesh.h>

#include <SDL2/SDL.h>

int main(int, char**)
{
    Renderer renderer;

    Camera camera;
    camera.position = linalg::aliases::float3(0.0, -2.0, -5.0f);
    renderer.add(&camera);

    {
        auto meshes = renderer.meshFactory("meshoui/resources/models/bricks.dae");

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

    renderer.remove(&camera);

    return 0;
}
