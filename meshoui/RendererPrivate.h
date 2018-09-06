#pragma once

#include "gltypes.h"
#include "hashid.h"
#include "Camera.h"
#include "Mesh.h"
#include "MeshLoader.h"

#include <SDL2/SDL.h>

#include <string>
#include <vector>

class ProgramUniform final
{
public:
    ~ProgramUniform();
    ProgramUniform();

    GLuint index;
    HashId name;
    GLint size;
    GLenum type;
    GLuint buffer;
    GLuint unit;
    GLuint enabler;
};
inline ProgramUniform::~ProgramUniform() {}
inline ProgramUniform::ProgramUniform() : index(0), size(0), type(0), buffer(0), unit(0), enabler(0) {}

class ProgramAttribute final
{
public:
    ~ProgramAttribute();
    ProgramAttribute();

    GLuint index;
    HashId name;
    GLint size;
    GLenum type;
};
inline ProgramAttribute::~ProgramAttribute() {}
inline ProgramAttribute::ProgramAttribute() : index(0), size(0), type(0) {}

class ProgramRegistration final
{
public:
    ~ProgramRegistration();
    ProgramRegistration();

    GLuint program;

    std::vector<ProgramAttribute> attributes;
    std::vector<ProgramUniform> uniforms;
    GLuint vertexArray;
};
inline ProgramRegistration::~ProgramRegistration() {}
inline ProgramRegistration::ProgramRegistration() : program(0), vertexArray(0) {}

class TextureRegistration final
{
public:
    ~TextureRegistration();
    TextureRegistration();
    TextureRegistration(HashId n);

    HashId name;
    GLuint buffer;
};
inline TextureRegistration::~TextureRegistration() {}
inline TextureRegistration::TextureRegistration() : buffer(0) {}
inline TextureRegistration::TextureRegistration(HashId n) : name(n), buffer(0) {}

class MeshRegistration final
{
public:
    ~MeshRegistration();
    MeshRegistration();
    MeshRegistration(HashId n);

    HashId definitionId;
    GLuint indexBuffer;
    GLuint vertexBuffer;
    size_t indexBufferSize;
    size_t referenceCount;
};
inline MeshRegistration::~MeshRegistration() {}
inline MeshRegistration::MeshRegistration() : indexBuffer(0), vertexBuffer(0), indexBufferSize(0), referenceCount(0) {}

class IGraphics;
class IUniform;
class Program;
typedef std::vector<std::pair<Program *, ProgramRegistration>> ProgramRegistrations;
typedef std::vector<MeshRegistration> MeshRegistrations;
typedef std::vector<TextureRegistration> TextureRegistrations;
class RendererPrivate final
{
public:
    static bool registerShader(GLenum shaderType, const std::string & shaderSource, GLuint & shader, std::string * const error);
    static void unregisterProgram(const ProgramRegistration & programRegistration);
    static bool registerProgram(Program * program, ProgramRegistration & programRegistration);
    static void bindProgram(const ProgramRegistration & programRegistration);
    static void unbindProgram(const ProgramRegistration &);
    static void unregisterMesh(const MeshRegistration & meshRegistration);
    static void registerMesh(const MeshDefinition & meshDefinition, MeshRegistration & meshRegistration);
    static void bindMesh(const MeshRegistration & meshRegistration, const ProgramRegistration & programRegistration);
    static void unbindMesh(const MeshRegistration & meshRegistration, const ProgramRegistration & programRegistration);

    ~RendererPrivate();
    RendererPrivate();

    void registerGraphics(Mesh * mesh);
    void registerGraphics(Program * program);
    void registerGraphics(Camera * cam);
    void unregisterGraphics(Mesh * mesh);
    void unregisterGraphics(Program * program);
    void unregisterGraphics(Camera * cam);
    void bindGraphics(Mesh * mesh);
    void bindGraphics(Program * program);
    void bindGraphics(Camera * cam, bool asLight = false);
    void unbindGraphics(Mesh * mesh);
    void unbindGraphics(Program * program);
    void unbindGraphics(Camera * cam);
    void setProgramUniforms(Mesh * mesh);
    void setProgramUniform(Program * program, IUniform * uniform);
    void unsetProgramUniform(Program * program, IUniform * uniform);
    void draw(Program * program, Mesh * mesh);
    const MeshFile & load(const std::string & filename);

    SDL_Window * window;
    SDL_GLContext glContext;
    GLenum glewError;

    ProgramRegistrations programRegistrations;
    MeshRegistrations meshRegistrations;
    TextureRegistrations textureRegistrations;
    MeshCache meshCache;

    bool toFullscreen;
    bool fullscreen;

    linalg::aliases::float4x4 projectionMatrix;
    Camera * camera;
    std::vector<Light *> lights;
};
inline RendererPrivate::~RendererPrivate() {}
