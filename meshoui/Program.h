#pragma once

#include "gltypes.h"
#include <linalg.h>
#include "hashid.h"

#include "IGraphics.h"
#include "IWhatever.h"

#include <string>
#include <vector>

class IUniform;
class Mesh;
class Program final
    : public IWhatever
    , public IGraphics
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

    // set before adding
    std::string vertexShaderSource;
    std::string fragmentShaderSource;

    // applied on next frame
    std::vector<IUniform *> uniforms;

    // outputs
    bool linked;
    std::string lastError;
};
inline Program::Program() : linked(false) {}
