#pragma once

#include <linalg.h>

#include <vector>

class Camera;
class Mesh;
class Program;
class RendererPrivate;
class Renderer final
{
public:
    virtual ~Renderer();
    Renderer(bool gles = false);

    void add(Mesh * mesh);
    void add(Program * program);
    void add(Camera * camera);
    void remove(Mesh * mesh);
    void remove(Program * program);
    void remove(Camera * camera);

    void update(double);
    void postUpdate();
    void renderMeshes();
    void renderWidgets();

    std::vector<Mesh *> meshFactory(const std::string &filename);

    linalg::aliases::float4x4 projectionMatrix;
    linalg::aliases::float2 sun;

    Program * defaultProgram;

private:
    RendererPrivate * d;
    std::vector<Camera *> cameras;
    std::vector<Mesh *> meshes;
    std::vector<Program *> programs;
};
