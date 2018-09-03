#pragma once

#include "gltypes.h"
#include <linalg.h>
#include "hashid.h"

#include "IGraphics.h"
#include "IWhatever.h"

#include <string>
#include <vector>

class IGraphicsUniform;
class Mesh;
class GraphicsProgram final
    : public IWhatever
    , public IGraphics
{
public:
    virtual ~GraphicsProgram();
    GraphicsProgram();

    void load(const std::string & filename);
    void add(IGraphicsUniform * uniform);
    void remove(IGraphicsUniform * uniform);
    void applyUniforms();
    void unapplyUniforms();
    void draw(Mesh * mesh);
    IGraphicsUniform * uniform(HashId name) const;

    // set before adding
    std::string vertexShaderSource;
    std::string fragmentShaderSource;

    // applied on next frame
    std::vector<IGraphicsUniform *> uniforms;

    // outputs
    bool linked;
    std::string lastError;
};
inline GraphicsProgram::GraphicsProgram() : linked(false) {}
