#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <Camera.h>
#include <Renderer.h>
#include <Program.h>
#include <Mesh.h>

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

template<typename T>
struct LinearVelocity
{
    LinearVelocity(T * t, float d = 0.f) : target(t), damping(d), linearVelocity(zero), linearAcceleration(zero) {}
    void step(float s)
    {
        float3 forward = qzdir(target->orientation);
        float3 right = qxdir(target->orientation);

        linearVelocity *= (1.0f - damping);
        linearVelocity += s * linearAcceleration;
        target->position += s * (linearVelocity.z * forward) + s * (linearVelocity.x * right);
    }
    T * target;
    float damping;
    float3 linearVelocity;
    float3 linearAcceleration;
};

template<typename T>
struct WASD
    : IKeyboard
{
    WASD(T * t) : target(t) {}
    virtual void keyAction(void *, int key, int, int action, int) override
    {
        if (action == GLFW_PRESS)
        {
            switch (key)
            {
            case GLFW_KEY_W:
                target->position.z -= 0.1;
                break;
            case GLFW_KEY_A:
                target->position.x -= 0.1;
                break;
            case GLFW_KEY_S:
                target->position.z += 0.1;
                break;
            case GLFW_KEY_D:
                target->position.x += 0.1;
                break;
            default:
                break;
            }
        }
    }
    T * target;
};

template<typename T>
struct WASD<LinearVelocity<T>>
    : IKeyboard
{
    WASD(LinearVelocity<T> * t) : target(t), w(false), a(false), s(false), d(false) {}
    virtual void keyAction(void *, int key, int, int action, int) override
    {
        static const float v = 100.f;
        if (action == GLFW_PRESS)
        {
            switch (key)
            {
            case GLFW_KEY_W:
                if (target->linearAcceleration.z > 0.f)
                    target->linearAcceleration.z = 0.f;
                target->linearAcceleration.z -= v;
                w = true;
                break;
            case GLFW_KEY_A:
                if (target->linearAcceleration.x > 0.f)
                    target->linearAcceleration.x = 0.f;
                target->linearAcceleration.x -= v;
                a = true;
                break;
            case GLFW_KEY_S:
                if (target->linearAcceleration.z < 0.f)
                    target->linearAcceleration.z = 0.f;
                target->linearAcceleration.z += v;
                s = true;
                break;
            case GLFW_KEY_D:
                if (target->linearAcceleration.x < 0.f)
                    target->linearAcceleration.x = 0.f;
                target->linearAcceleration.x += v;
                d = true;
                break;
            default:
                break;
            }
        }
        if (action == GLFW_RELEASE)
        {
            switch (key)
            {
            case GLFW_KEY_W:
                target->linearAcceleration.z = 0.0;
                w = false;
                if (s) target->linearAcceleration.z += v;
                break;
            case GLFW_KEY_A:
                target->linearAcceleration.x = 0.0;
                a = false;
                if (d) target->linearAcceleration.x += v;
                break;
            case GLFW_KEY_S:
                target->linearAcceleration.z = 0.0;
                s = false;
                if (w) target->linearAcceleration.z -= v;
                break;
            case GLFW_KEY_D:
                target->linearAcceleration.x = 0.0;
                d = false;
                if (a) target->linearAcceleration.x -= v;
                break;
            default:
                break;
            }
        }
    }
    LinearVelocity<T> * target;
    bool w,a,s,d;
};

template<typename T>
struct Mouselook
    : IMouse
{
    Mouselook(T * t) : target(t), previousX(0), previousY(0), once(false) {}
    virtual void cursorPositionAction(void *, double xpos, double ypos) override
    {
        if (once)
        {
            double deltaX = xpos - previousX;
            double deltaY = ypos - previousY;

            static const float rotationScaler = 0.0005f;
            {
                float3 right = qxdir(target->orientation);
                target->orientation = qmul(rotation_quat(right, float(-deltaY * rotationScaler * M_PI/2)), target->orientation);
            }
            {
                float3 up(0.,1.,0.);
                target->orientation = qmul(rotation_quat(up, float(-deltaX * rotationScaler * M_PI)), target->orientation);
            }
        }

        previousX = xpos;
        previousY = ypos;
        once = true;
    }
    virtual void mouseLost() override
    {
        once = false;
    }
    T * target;
    double previousX, previousY;
    bool once;
};

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
