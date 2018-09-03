#pragma once

#include "hashid.h"

#include <string>
#include <vector>

class IUniform;
class Mesh;
class RendererPrivate;
class Program final
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
    std::string vertexShaderSource;
    std::string fragmentShaderSource;

    // applied on next frame
    std::vector<IUniform *> uniforms;

    // outputs
    bool linked;
    std::string lastError;

private:
    friend class RendererPrivate;
    RendererPrivate * d;
};
inline Program::Program() : linked(false) {}
inline RendererPrivate *Program::d_ptr() const { return d; }
