#pragma once

#include "Mesh.h"
#include <linalg.h>

#include <vector>

class Vertex;
class Camera;
class Program;
class Widget;
class RendererPrivate;
class Renderer final
{
public:
    virtual ~Renderer();
    Renderer(bool gles = false);

    void addedToApplication();
    void aboutToBeRemovedFromApplication();
    void add(IWhatever * whatever);
    void remove(IWhatever * whatever);

    void update(double);
    void postUpdate();
    void renderMeshes();
    void renderWidgets();

    std::vector<Mesh *> meshFactory(const std::string &filename);

    linalg::aliases::float4x4 projectionMatrix;
    linalg::aliases::float2 sun;

    Program * defaultProgram;

private:
    void dispatch(Mesh * mesh);

    RendererPrivate * d;
    std::vector<Camera *> cameras;
    std::vector<Mesh *> meshes;
    std::vector<Program *> programs;
};
