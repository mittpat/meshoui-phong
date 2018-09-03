#include <Camera.h>
#include <GraphicsModule.h>
#include <GraphicsProgram.h>
#include <GraphicsUniform.h>
#include <Mesh.h>

#include <SDL2/SDL.h>

int main(int, char**)
{
    GraphicsModule graphicsModule;

    Camera camera;
    camera.position = linalg::aliases::float3(0.0, -2.0, -5.0f);
    graphicsModule.add(&camera);

    {
        auto meshes = graphicsModule.meshFactory("meshoui/resources/models/bricks.dae");

        bool run = true;
        while (run)
        {
            SDL_PumpEvents();
            SDL_Event * event = nullptr;
            if (0 != SDL_PeepEvents(event, 8, SDL_GETEVENT, SDL_QUIT, SDL_QUIT))
                run = false;

            graphicsModule.update(0.016);
        }

        for (auto mesh : meshes) graphicsModule.remove(mesh);
    }

    graphicsModule.remove(&camera);

    return 0;
}
