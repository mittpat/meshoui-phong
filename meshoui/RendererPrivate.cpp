#include "Mesh.h"
#include "Program.h"
#include "RendererPrivate.h"
#include "TextureLoader.h"
#include "Uniform.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <loose.h>
#include <algorithm>
#include <experimental/filesystem>
#include <fstream>

#ifndef GL_FLOAT_MAT4_ARB
#define GL_FLOAT_MAT4_ARB GL_FLOAT_MAT4
#endif

using namespace linalg;
using namespace linalg::aliases;
namespace std { namespace filesystem = experimental::filesystem; }
using namespace Meshoui;

namespace
{
    const ProgramRegistration & registrationFor(const ProgramRegistrations & programRegistrations, Program * program)
    {
        auto found = std::find_if(programRegistrations.begin(), programRegistrations.end(), [program](const std::pair<Program*, ProgramRegistration> & pair)
        {
            return pair.first == program;
        });
        if (found != programRegistrations.end())
        {
            return found->second;
        }
        static const ProgramRegistration Invalid;
        return Invalid;
    }

    const MeshRegistration & registrationFor(const MeshRegistrations & meshRegistrations, Mesh * mesh)
    {
        if (mesh != nullptr)
        {
            auto found = std::find_if(meshRegistrations.begin(), meshRegistrations.end(), [mesh](const MeshRegistration & meshRegistration)
            {
                return meshRegistration.definitionId == mesh->definitionId;
            });
            if (found != meshRegistrations.end())
            {
                return *found;
            }
        }
        static const MeshRegistration Invalid;
        return Invalid;
    }

    void texture(GLuint * buffer, const std::string & filename, bool repeat)
    {
        if (TextureLoader::loadDDS(buffer, std::filesystem::path(filename).replace_extension(".dds"), repeat))
            return;
        if (TextureLoader::loadPNG(buffer, std::filesystem::path(filename).replace_extension(".png"), repeat))
            return;
    }

    void setUniform(const TextureRegistrations & textureRegistrations, Mesh * mesh, IUniform * uniform, const ProgramUniform & programUniform)
    {
        GLenum type = programUniform.type;
        switch (type)
        {
        case GL_FLOAT_VEC2_ARB:
            glUniform2fv(programUniform.index, 1, (float*)uniform->data());
            break;
        case GL_FLOAT_VEC3_ARB:
            glUniform3fv(programUniform.index, 1, (float*)uniform->data());
            break;
        case GL_FLOAT_VEC4_ARB:
            glUniform4fv(programUniform.index, 1, (float*)uniform->data());
            break;
        case GL_FLOAT_MAT4_ARB:
            glUniformMatrix4fv(programUniform.index, 1, GL_FALSE, (float*)uniform->data());
            break;
        case GL_SAMPLER_2D_ARB:
            if (programUniform.buffer > 0)
            {
                glUniform1i(programUniform.enabler, true);
                glUniform1i(programUniform.index, programUniform.unit);
                glActiveTexture(GL_TEXTURE0 + programUniform.unit);
                glBindTexture(GL_TEXTURE_2D, programUniform.buffer);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);
            }
            else
            {
                HashId textureName = dynamic_cast<UniformSampler2D *>(uniform)->filename;
                auto textureRegistration = std::find_if(textureRegistrations.begin(), textureRegistrations.end(), [textureName](const TextureRegistration & textureRegistration)
                {
                    return textureRegistration.name == textureName;
                });
                if (textureRegistration != textureRegistrations.end()
                    && (*textureRegistration).buffer > 0)
                {
                    glUniform1i(programUniform.enabler, true);
                    glUniform1i(programUniform.index, programUniform.unit);
                    glActiveTexture(GL_TEXTURE0 + programUniform.unit);
                    glBindTexture(GL_TEXTURE_2D, (*textureRegistration).buffer);

                    Render::Flags flags = mesh ? mesh->renderFlags : Render::Default;
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, flags & Render::Mipmap ? GL_LINEAR_MIPMAP_LINEAR : (flags & Render::Filtering ? GL_LINEAR : GL_NEAREST));
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, flags & Render::Filtering ? GL_LINEAR : GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, flags & Render::Anisotropic ? 16 : 1);
                }
                else
                {
                    glUniform1i(programUniform.enabler, false);
                }
            }
            break;
        case GL_INT:
            glUniform1iv(programUniform.index, programUniform.size, (int*)uniform->data());
            break;
        default:
            break;
        }
    }

    bool registerShader(GLenum shaderType, const std::vector<std::string> & defines, const std::string & shaderSource, GLuint & shader, std::string * const error)
    {
        shader = glCreateShader(shaderType);
        std::vector<const char *> shaderSource_str;
        for (const auto & def : defines)
            shaderSource_str.push_back(def.c_str());
        shaderSource_str.push_back(shaderSource.c_str());

        glShaderSource(shader, shaderSource_str.size(), shaderSource_str.data(), nullptr);
        glCompileShader(shader);

        GLint shaderCompiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &shaderCompiled);
        if (shaderCompiled != GL_TRUE)
        {
            if (error != nullptr)
            {
                GLint infoLogLength = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength );
                error->resize(infoLogLength);
                glGetShaderInfoLog(shader, infoLogLength, &infoLogLength, &(*error)[0]);
            }

            glDeleteShader(shader);
            return false;
        }
        return true;
    }
}

void RendererPrivate::unregisterProgram(const ProgramRegistration & programRegistration)
{
    if (glIsProgram(programRegistration.program))
    {
        glDeleteProgram(programRegistration.program);
    }
    for (auto uniform : programRegistration.uniforms)
    {
        glDeleteTextures(1, &uniform.buffer);
    }
    glDeleteVertexArrays(1, &programRegistration.vertexArray);
}

bool RendererPrivate::registerProgram(Program * program, ProgramRegistration & programRegistration)
{
    GLuint vertex, fragment;
    if (!registerShader(GL_VERTEX_SHADER, program->defines, program->vertexShaderSource, vertex, &program->lastError))
        return false;

    if (!registerShader(GL_FRAGMENT_SHADER, program->defines, program->fragmentShaderSource, fragment, &program->lastError))
        return false;

    programRegistration.program = glCreateProgram();
    glAttachShader(programRegistration.program, vertex);
    glAttachShader(programRegistration.program, fragment);
    glLinkProgram(programRegistration.program);

    glDetachShader(programRegistration.program, vertex);
    glDetachShader(programRegistration.program, fragment);

    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint programSuccess = GL_FALSE;
    glGetProgramiv(programRegistration.program, GL_LINK_STATUS, &programSuccess);
    if (programSuccess != GL_TRUE)
    {
        {
            GLint infoLogLength = 0;
            glGetProgramiv(programRegistration.program, GL_INFO_LOG_LENGTH, &infoLogLength);
            program->lastError.resize(infoLogLength);
            glGetProgramInfoLog(programRegistration.program, infoLogLength, &infoLogLength, &program->lastError[0]);
        }

        unregisterProgram(programRegistration);
        program->linked = false;
    }
    else
    {
        GLuint unit = 0;
        GLint count = 0;
        glGetProgramiv(programRegistration.program, GL_ACTIVE_UNIFORMS, &count);
        programRegistration.uniforms.resize(count);
        for (GLint i = 0; i < count; i++)
        {
            std::string name;
            GLsizei nameLength = 32;
            name.resize(nameLength);
            glGetActiveUniform(programRegistration.program, i,
                               nameLength, &nameLength, &programRegistration.uniforms[i].size,
                               &programRegistration.uniforms[i].type, &name[0]);
            name.resize(nameLength);
            programRegistration.uniforms[i].name = name;
            programRegistration.uniforms[i].index = glGetUniformLocation(programRegistration.program, name.c_str());

            if (!program->uniform(name))
            {
                if (auto uniform = UniformFactory::makeUniform(name, programRegistration.uniforms[i].type, programRegistration.uniforms[i].size))
                {
                    program->add(uniform);
                }
            }

            if (auto sampler = dynamic_cast<UniformSampler2D *>(program->uniform(name)))
            {
                programRegistration.uniforms[i].unit = unit++;
                programRegistration.uniforms[i].enabler = glGetUniformLocation(programRegistration.program, (name + "Active").c_str());
                if (!sampler->filename.empty())
                {
                    texture(&programRegistration.uniforms[i].buffer, sampler->filename, true);
                }
            }
        }

        glGetProgramiv(programRegistration.program, GL_ACTIVE_ATTRIBUTES, &count);
        programRegistration.attributes.resize(count);
        for (GLint i = 0; i < count; i++)
        {
            std::string name;
            GLsizei nameLength = 16;
            name.resize(nameLength);
            glGetActiveAttrib(programRegistration.program, i,
                              nameLength, &nameLength, &programRegistration.attributes[i].size,
                              &programRegistration.attributes[i].type, &name[0]);
            name.resize(nameLength);
            programRegistration.attributes[i].name = name;
            programRegistration.attributes[i].index = glGetAttribLocation(programRegistration.program, name.c_str());
        }

        glGenVertexArrays(1, &programRegistration.vertexArray);
    }

    program->linked = true;
    return program->linked;
}

void RendererPrivate::bindProgram(const ProgramRegistration & programRegistration)
{
    glUseProgram(programRegistration.program);
    glBindVertexArray(programRegistration.vertexArray);
}

void RendererPrivate::unbindProgram(const ProgramRegistration & programRegistration)
{
    glBindVertexArray(0);
    glUseProgram(0);
}

void RendererPrivate::unregisterMesh(const MeshRegistration & meshRegistration)
{
    if (meshRegistration.indexBuffer != 0)
        glDeleteBuffers(1, &meshRegistration.indexBuffer);
    if (meshRegistration.vertexBuffer != 0)
        glDeleteBuffers(1, &meshRegistration.vertexBuffer);
}

void RendererPrivate::registerMesh(const MeshDefinition & meshDefinition, MeshRegistration & meshRegistration)
{
    if (!meshDefinition.indices.empty())
    {
        glGenBuffers(1, &meshRegistration.indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshRegistration.indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshDefinition.indices.size() * sizeof(unsigned int), nullptr, GL_STATIC_DRAW);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, meshDefinition.indices.size() * sizeof(unsigned int), meshDefinition.indices.data());
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    glGenBuffers(1, &meshRegistration.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, meshRegistration.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, meshDefinition.vertices.size() * Vertex::AttributeDataSize, nullptr, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, meshDefinition.vertices.size() * Vertex::AttributeDataSize, meshDefinition.vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    meshRegistration.indexBufferSize = meshDefinition.indices.size();
    meshRegistration.vertexBufferSize = meshDefinition.vertices.size();
    meshRegistration.definitionId = meshDefinition.definitionId;
}

void RendererPrivate::bindMesh(const MeshRegistration & meshRegistration, const ProgramRegistration & programRegistration)
{
    if (meshRegistration.indexBuffer != 0)
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshRegistration.indexBuffer);

    if (meshRegistration.vertexBuffer != 0)
        glBindBuffer(GL_ARRAY_BUFFER, meshRegistration.vertexBuffer);

    size_t offset = 0;
    for (const Attribute & attributeDef : Vertex::Attributes)
    {
        auto found = std::find_if(programRegistration.attributes.begin(), programRegistration.attributes.end(), [attributeDef](const ProgramAttribute & attribute)
        {
            return attribute.name == attributeDef.name;
        });
        if (found != programRegistration.attributes.end())
        {
            glEnableVertexAttribArray((*found).index);
            glVertexAttribPointer((*found).index, attributeDef.size, GL_FLOAT, GL_FALSE, Vertex::AttributeDataSize, (void*)offset);
        }
        offset += attributeDef.size * sizeof(GLfloat);
    }
}

void RendererPrivate::unbindMesh(const MeshRegistration &, const ProgramRegistration & programRegistration)
{
    for (const Attribute & attributeDef : Vertex::Attributes)
    {
        auto found = std::find_if(programRegistration.attributes.begin(), programRegistration.attributes.end(), [attributeDef](const ProgramAttribute & attribute)
        {
            return attribute.name == attributeDef.name;
        });
        if (found != programRegistration.attributes.end())
        {
            glDisableVertexAttribArray((*found).index);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

RendererPrivate::RendererPrivate() 
    : window(nullptr)
    , toFullscreen(false)
    , fullscreen(false)
    , projectionMatrix(linalg::perspective_matrix(degreesToRadians(100.f), 1920/1080.f, 0.1f, 1000.f))
    , camera(nullptr)
{
    
}

void RendererPrivate::registerGraphics(Model *model)
{
    model->d = this;
    load(model->filename);
}

void RendererPrivate::registerGraphics(Mesh * mesh)
{
    mesh->d = this;
    const MeshRegistration * meshRegistration = &registrationFor(meshRegistrations, mesh);
    const_cast<MeshRegistration *>(meshRegistration)->referenceCount += 1;
}

void RendererPrivate::registerGraphics(Program * program)
{
    program->d = this;
    ProgramRegistration programRegistration;
    registerProgram(program, programRegistration);
    programRegistrations.push_back(std::make_pair(program, programRegistration));
}

void RendererPrivate::registerGraphics(Camera * cam)
{
    cam->d = this;
}

void RendererPrivate::registerGraphics(const MeshFile &meshFile)
{
    for (const auto & definition : meshFile.definitions)
    {
        printf("%s\n", definition.definitionId.str.c_str());

        MeshRegistration meshRegistration;
        registerMesh(definition, meshRegistration);
        meshRegistrations.push_back(meshRegistration);
    }
    for (const auto & material : meshFile.materials)
    {
        for (auto value : material.values)
        {
            if (value.texture)
            {
                TextureRegistration textureRegistration = TextureRegistration(*value.texture);
                texture(&textureRegistration.buffer, sibling(*value.texture, meshFile.filename), material.repeatTexcoords);
                textureRegistrations.push_back(textureRegistration);
            }
        }
    }
}

void RendererPrivate::unregisterGraphics(Model *model)
{
    model->d = nullptr;
}

void RendererPrivate::unregisterGraphics(Mesh * mesh)
{
    auto found = std::find_if(meshRegistrations.begin(), meshRegistrations.end(), [mesh](const MeshRegistration & meshRegistration)
    {
        return meshRegistration.definitionId == mesh->definitionId;
    });
    if (found != meshRegistrations.end())
    {
        MeshRegistration & meshRegistration = *found;
        if (meshRegistration.referenceCount > 0)
            meshRegistration.referenceCount--;
        if (meshRegistration.referenceCount == 0)
        {
            unregisterMesh(meshRegistration);
            meshRegistrations.erase(found);
        }
    }
    for (auto * uniform : mesh->uniforms) { delete uniform; }
    mesh->uniforms.clear();
    mesh->d = nullptr;
}

void RendererPrivate::unregisterGraphics(Program * program)
{
    auto found = std::find_if(programRegistrations.begin(), programRegistrations.end(), [program](const std::pair<Program*, ProgramRegistration> & pair)
    {
        return pair.first == program;
    });
    if (found != programRegistrations.end())
    {
        unregisterProgram(found->second);
        programRegistrations.erase(found);
    }
    program->d = nullptr;
}

void RendererPrivate::unregisterGraphics(Camera *cam)
{
    unbindGraphics(cam);
    cam->d = nullptr;
}

void RendererPrivate::bindGraphics(Mesh * mesh)
{
    bindMesh(registrationFor(meshRegistrations, mesh), registrationFor(programRegistrations, mesh->program));
}

void RendererPrivate::bindGraphics(Program * program)
{
    bindProgram(registrationFor(programRegistrations, program));
}

void RendererPrivate::bindGraphics(Camera *cam, bool asLight)
{
    if (asLight && std::find(lights.begin(), lights.end(), cam) == lights.end())
        lights.push_back(cam);
    else
        camera = cam;
}

void RendererPrivate::unbindGraphics(Mesh * mesh)
{
    unbindMesh(registrationFor(meshRegistrations, mesh), registrationFor(programRegistrations, mesh->program));
}

void RendererPrivate::unbindGraphics(Program * program)
{
    unbindProgram(registrationFor(programRegistrations, program));
}

void RendererPrivate::unbindGraphics(Camera *cam)
{
    if (camera == cam)
        camera = nullptr;
    if (std::find(lights.begin(), lights.end(), cam) != lights.end())
        lights.erase(std::remove(lights.begin(), lights.end(), cam));
}

void RendererPrivate::setProgramUniforms(Mesh * mesh)
{
    const ProgramRegistration & programRegistration = registrationFor(programRegistrations, mesh->program);
    for (const ProgramUniform & programUniform : programRegistration.uniforms)
    {
        if (auto * uniform = mesh->uniform(programUniform.name))
        {
            setUniform(textureRegistrations, mesh, uniform, programUniform);
        }
    }
}

void RendererPrivate::setProgramUniform(Program * program, IUniform * uniform)
{
    const ProgramRegistration & programRegistration = registrationFor(programRegistrations, program);
    auto programUniform = std::find_if(programRegistration.uniforms.begin(), programRegistration.uniforms.end(), [uniform](const ProgramUniform & programUniform)
    {
        return programUniform.name == uniform->name;
    });
    if (programUniform != programRegistration.uniforms.end())
    {
        setUniform(textureRegistrations, nullptr, uniform, *programUniform);
    }
}

void RendererPrivate::unsetProgramUniform(Program *program, IUniform * uniform)
{
    const ProgramRegistration & programRegistration = registrationFor(programRegistrations, program);
    auto programUniform = std::find_if(programRegistration.uniforms.begin(), programRegistration.uniforms.end(), [uniform](const ProgramUniform & programUniform)
    {
        return programUniform.name == uniform->name;
    });
    if (programUniform != programRegistration.uniforms.end())
    {
        static const std::array<GLfloat, 16> blank = {0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f,0.f};

        GLenum type = (*programUniform).type;
        switch (type)
        {
        case GL_FLOAT:
            glUniform1fv((*programUniform).index, 1, blank.data());
            break;
        case GL_FLOAT_VEC2_ARB:
            glUniform2fv((*programUniform).index, 1, blank.data());
            break;
        case GL_FLOAT_VEC3_ARB:
            glUniform3fv((*programUniform).index, 1, blank.data());
            break;
        case GL_FLOAT_VEC4_ARB:
            glUniform4fv((*programUniform).index, 1, blank.data());
            break;
        case GL_FLOAT_MAT4_ARB:
            glUniformMatrix4fv((*programUniform).index, 1, GL_FALSE, blank.data());
            break;
        case GL_SAMPLER_2D_ARB:
            glUniform1i((*programUniform).enabler, false);
            glUniform1i((*programUniform).index, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            break;
        case GL_INT:
            glUniform1iv((*programUniform).index, (*programUniform).size, (int*)blank.data());
            break;
        default:
            break;
        }
    }
}

void RendererPrivate::draw(Program *, Mesh * mesh)
{
    if (mesh->renderFlags & Render::Points)
    {
        glDrawArrays(GL_POINTS, 0, registrationFor(meshRegistrations, mesh).vertexBufferSize);
    }
    else
    {
        glDrawElements(GL_TRIANGLES, registrationFor(meshRegistrations, mesh).indexBufferSize, GL_UNSIGNED_INT, 0);
    }
}

void RendererPrivate::fill(const std::string &filename, const std::vector<Mesh *> &meshes)
{
    const MeshFile & meshFile = load(filename);
    for (size_t i = 0; i < meshFile.instances.size(); ++i)
    {
        const MeshInstance & instance = meshFile.instances[i];
        Mesh * mesh = meshes[i];
        mesh->instanceId = instance.instanceId;
        mesh->definitionId = instance.definitionId;
        mesh->filename = meshFile.filename;
        mesh->scale = instance.scale;
        mesh->position = instance.position;
        mesh->orientation = instance.orientation;
        auto definition = std::find_if(meshFile.definitions.begin(), meshFile.definitions.end(), [instance](const auto & definition){ return definition.definitionId == instance.definitionId; });
        if (definition->doubleSided)
        {
            mesh->renderFlags &= ~Render::BackFaceCulling;
        }
        auto material = std::find_if(meshFile.materials.begin(), meshFile.materials.end(), [instance](const MeshMaterial & material) { return material.name == instance.materialId; });
        for (auto value : material->values)
        {
            IUniform * uniform = nullptr;
            if (value.texture)
            {
                uniform = new UniformSampler2D(value.sid, *value.texture);
            }
            else
            {
                uniform = UniformFactory::makeUniform(value.sid, enumForVectorSize(value.data->size()));
                uniform->setData(value.data->data());
            }
            mesh->add(uniform);
        }
    }
}

const MeshFile& RendererPrivate::load(const std::string &filename)
{
    auto foundFile = std::find_if(meshFiles.begin(), meshFiles.end(), [filename](const MeshFile &meshFile)
    {
        return meshFile.filename == filename;
    });
    if (foundFile == meshFiles.end())
    {
        MeshFile meshFile;
        if (MeshLoader::load(filename, meshFile))
        {
            registerGraphics(meshFile);
            meshFiles.push_back(meshFile);
            return meshFiles.back();
        }
        else
        {
            static const MeshFile Invalid;
            return Invalid;
        }
    }
    else
    {
        return *foundFile;
    }
}
