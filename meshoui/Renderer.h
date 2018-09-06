#pragma once

#include <linalg.h>

#include <algorithm>
#include <vector>

class Camera;
class Mesh;
class Program;
class Widget;
class RendererPrivate;
class Renderer final
{
public:
    virtual ~Renderer();
    Renderer(bool gles = false);

    void add(Mesh * mesh);
    void add(Program * program);
    void add(Camera * camera);
    void add(Widget * widget);
    void remove(Mesh * mesh);
    void remove(Program * program);
    void remove(Camera * camera);
    void remove(Widget * widget);

    void update(double);
    void postUpdate();
    void renderMeshes();
    void renderWidgets();

    bool load(const std::string & filename, size_t & count);
    void fill(const std::string & filename, std::vector<Mesh *> & m);

    template <typename T>
    std::vector<T *> meshFactory(const std::string & filename);

    Program * defaultProgram;

private:
    RendererPrivate * d;
    std::vector<Camera *> cameras;
    std::vector<Mesh *> meshes;
    std::vector<Program *> programs;
    std::vector<Widget *> widgets;
};

template<typename T>
std::vector<T *> Renderer::meshFactory(const std::string & filename)
{
    std::vector<Mesh *> built;
    size_t count;
    if (load(filename, count))
    {
        built.reserve(count);
        for (size_t i = 0; i < count; ++i)
            built.emplace_back(new T());
        fill(filename, built);
    }
    std::vector<T*> ret;
    std::transform(built.begin(), built.end(), std::back_inserter(ret), [](Mesh * mesh){ return dynamic_cast<T*>(mesh); });
    return ret;
}
