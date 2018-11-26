#pragma once

#include <hashid.h>

#include <string>
#include <vector>

namespace Meshoui
{
    class Mesh;
    class RendererPrivate;
    class Program
    {
    public:
        virtual ~Program();
        Program();

        void load(const std::string & filename);
        void draw(Mesh * mesh);
        RendererPrivate * d_ptr() const;

        // set before adding
        std::vector<char> vertexShaderSource;
        std::vector<char> fragmentShaderSource;

    private:
        friend class RendererPrivate;
        RendererPrivate * d;
    };
    inline Program::Program() {}
    inline RendererPrivate *Program::d_ptr() const { return d; }
}
