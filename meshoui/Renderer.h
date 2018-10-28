#pragma once

#include <linalg.h>

#include <algorithm>
#include <vector>

#include "Mesh.h"

class Camera;
class MeshFile;
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
    void remove(Model * model);
    void remove(Mesh * mesh);
    void remove(Program * program);
    void remove(Camera * camera);
    void remove(Widget * widget);

    void update(float s);
    void postUpdate();
    void renderMeshes();
    void renderWidgets();

    RendererPrivate * d;

    Program * defaultProgram;
    float time;

private:
    std::vector<Camera *> cameras;
    std::vector<Mesh *> meshes;
    std::vector<Model *> models;
    std::vector<Program *> programs;
    std::vector<Widget *> widgets;
};
