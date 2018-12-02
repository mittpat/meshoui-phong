#pragma once

#include <hashid.h>

#include <string>
#include <vector>

#include "enums.h"

namespace Meshoui
{
    class Mesh;
    class RendererPrivate;
    class Program
    {
    public:
        virtual ~Program();
        Program();
        Program(const std::string & filename);

        void load(const std::string & filename);
        void draw(Mesh * mesh);
        RendererPrivate * d_ptr() const;

        // set before adding
        std::vector<char> vertexShaderSource;
        std::vector<char> fragmentShaderSource;
        Feature::Flags features;

    private:
        friend class RendererPrivate;
        RendererPrivate * d;
    };
    inline Program::Program() : features(Feature::Default) {}
    inline Program::Program(const std::string & filename) : features(Feature::Default) { load(filename); }
    inline RendererPrivate *Program::d_ptr() const { return d; }
}
