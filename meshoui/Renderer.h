#pragma once

#include <linalg.h>

#include <algorithm>
#include <vector>

#include "IKeyboard.h"
#include "IMouse.h"
#include "Mesh.h"

namespace Meshoui
{
    struct GlfwCallbacks;
    struct WidgetCallbacks;

    class Camera;
    class Program;
    class Widget;
    class RendererPrivate;
    class Renderer final
    {
    public:
        virtual ~Renderer();
        Renderer();

        bool shouldClose() const;

        void add(Model * model);
        void add(Mesh * mesh);
        void add(Program * program);
        void add(Camera * camera);
        void add(Widget * widget);
        void add(IKeyboard * keyboard);
        void add(IMouse * mouse);
        void remove(Model * model);
        void remove(Mesh * mesh);
        void remove(Program * program);
        void remove(Camera * camera);
        void remove(Widget * widget);
        void remove(IKeyboard * keyboard);
        void remove(IMouse * mouse);

        void update(float s);
        void postUpdate();
        void renderMeshes();
        void renderWidgets();

        RendererPrivate * d;

        Program * defaultProgram;
        float time;
        bool overlay;

    private:
        std::vector<Camera *> cameras;
        std::vector<IKeyboard *> keyboards;
        std::vector<IMouse *> mice;
        std::vector<Mesh *> meshes;
        std::vector<Model *> models;
        std::vector<Program *> programs;
        std::vector<Widget *> widgets;

        friend class RendererPrivate;
        friend struct GlfwCallbacks;
        WidgetCallbacks * widgetCallbacks;
    };
}
