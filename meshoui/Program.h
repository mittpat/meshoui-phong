#pragma once

#include <hashid.h>

#include <string>
#include <vector>

namespace Meshoui
{
    class IUniform;
    class Mesh;
    class RendererPrivate;
    class Program
    {
    public:
        virtual ~Program();
        Program();

        void load(const std::string & filename);
        void add(IUniform * uniform);
        void remove(IUniform * uniform);
        void applyUniforms();
        void unapplyUniforms();
        void draw(Mesh * mesh);
        IUniform * uniform(HashId name) const;
        RendererPrivate * d_ptr() const;

        // set before adding
        std::vector<char> vertexShaderSource;
        std::vector<char> vertexShaderReflection;
        std::vector<char> fragmentShaderSource;
        std::vector<char> fragmentShaderReflection;

        // applied on next frame
        std::vector<IUniform *> uniforms;

    private:
        friend class RendererPrivate;
        RendererPrivate * d;
    };
    inline Program::Program() {}
    inline RendererPrivate *Program::d_ptr() const { return d; }
}
