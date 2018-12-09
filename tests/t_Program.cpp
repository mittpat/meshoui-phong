#include "test.h"

#include <Renderer.h>
#include <Program.h>

using namespace Meshoui;

int main(int, char**)
{
    Renderer renderer;

    Program program;
    program.load("../meshoui/resources/shaders/Phong.shader");
    renderer.add(&program);

    {
        int counter = 0;
        while (counter++ < 10)
        {
            renderer.update(0.016f);
        }
    }

    renderer.remove(&program);

    return 0;
}
