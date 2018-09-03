#include <GL/glew.h>

#include "GraphicsPrivate.h"
#include "GraphicsProgram.h"
#include "GraphicsUniform.h"
#include "Mesh.h"
#include "TextureLoader.h"

#include <SDL2/SDL_image.h>

#include "loose.h"
#include <algorithm>
#include <experimental/filesystem>
#include <fstream>

namespace std { namespace filesystem = experimental::filesystem; }

namespace
{
    const ProgramRegistration & registrationFor(const GraphicsProgramRegistrations & programRegistrations, GraphicsProgram * graphicsProgram)
    {
        auto found = std::find_if(programRegistrations.begin(), programRegistrations.end(), [graphicsProgram](const std::pair<GraphicsProgram*, ProgramRegistration> & pair)
        {
            return pair.first == graphicsProgram;
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

    void texture(GLuint * buffer, const std::string & filename)
    {
        if (TextureLoader::loadDDS(buffer, std::filesystem::path(filename).replace_extension(".dds")))
            return;
        if (TextureLoader::loadPNG(buffer, std::filesystem::path(filename).replace_extension(".png")))
            return;
    }

    void setUniform(const TextureRegistrations & textureRegistrations, Mesh * mesh, IGraphicsUniform * uniform, const ProgramUniform & programUniform)
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
                HashId textureName = dynamic_cast<GraphicsUniformSampler2D *>(uniform)->filename;
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
}

bool GraphicsPrivate::registerShader(GLenum shaderType, const std::string & shaderSource, GLuint & shader, std::string * const error)
{
    shader = glCreateShader(shaderType);
    const char * shaderSource_str = shaderSource.c_str();
    glShaderSource(shader, 1, &shaderSource_str, nullptr);
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

void GraphicsPrivate::unregisterProgram(const ProgramRegistration & programRegistration)
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

bool GraphicsPrivate::registerProgram(GraphicsProgram * graphicsProgram, ProgramRegistration & programRegistration)
{
    GLuint vertex, fragment;
    if (!registerShader(GL_VERTEX_SHADER, graphicsProgram->vertexShaderSource, vertex, &graphicsProgram->lastError))
        return false;

    if (!registerShader(GL_FRAGMENT_SHADER, graphicsProgram->fragmentShaderSource, fragment, &graphicsProgram->lastError))
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
            graphicsProgram->lastError.resize(infoLogLength);
            glGetProgramInfoLog(programRegistration.program, infoLogLength, &infoLogLength, &graphicsProgram->lastError[0]);
        }

        unregisterProgram(programRegistration);
        graphicsProgram->linked = false;
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

            if (!graphicsProgram->uniform(name))
            {
                if (auto uniform = GraphicsUniformFactory::makeUniform(name, programRegistration.uniforms[i].type, programRegistration.uniforms[i].size))
                {
                    graphicsProgram->add(uniform);
                }
            }

            if (auto sampler = dynamic_cast<GraphicsUniformSampler2D *>(graphicsProgram->uniform(name)))
            {
                programRegistration.uniforms[i].unit = unit++;
                programRegistration.uniforms[i].enabler = glGetUniformLocation(programRegistration.program, (name + "Active").c_str());
                if (!sampler->filename.empty())
                {
                    texture(&programRegistration.uniforms[i].buffer, sampler->filename);
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

    graphicsProgram->linked = true;
    return graphicsProgram->linked;
}

void GraphicsPrivate::bindProgram(const ProgramRegistration & programRegistration)
{
    glUseProgram(programRegistration.program);
    glBindVertexArray(programRegistration.vertexArray);
}

void GraphicsPrivate::unbindProgram(const ProgramRegistration & programRegistration)
{
    glBindVertexArray(0);
    glUseProgram(0);
}

void GraphicsPrivate::unregisterMesh(const MeshRegistration & meshRegistration)
{
    glDeleteBuffers(1, &meshRegistration.indexBuffer);
    glDeleteBuffers(1, &meshRegistration.vertexBuffer);
}

void GraphicsPrivate::registerMesh(const MeshDefinition & meshDefinition, MeshRegistration & meshRegistration)
{
    glGenBuffers(1, &meshRegistration.indexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshRegistration.indexBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshDefinition.indices.size() * sizeof(unsigned int), nullptr, GL_STATIC_DRAW);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, meshDefinition.indices.size() * sizeof(unsigned int), meshDefinition.indices.data());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenBuffers(1, &meshRegistration.vertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, meshRegistration.vertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, meshDefinition.vertices.size() * Vertex::AttributeDataSize, nullptr, GL_STATIC_DRAW);
    glBufferSubData(GL_ARRAY_BUFFER, 0, meshDefinition.vertices.size() * Vertex::AttributeDataSize, meshDefinition.vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    meshRegistration.indexBufferSize = meshDefinition.indices.size();
    meshRegistration.definitionId = meshDefinition.definitionId;
}

void GraphicsPrivate::bindMesh(const MeshRegistration & meshRegistration, const ProgramRegistration & programRegistration)
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshRegistration.indexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, meshRegistration.vertexBuffer);

    size_t offset = 0;
    for (const HashId & attributeName : Vertex::Attributes)
    {
        auto found = std::find_if(programRegistration.attributes.begin(), programRegistration.attributes.end(), [attributeName](const ProgramAttribute & attribute)
        {
            return attribute.name == attributeName;
        });
        if (found != programRegistration.attributes.end())
        {
            glEnableVertexAttribArray((*found).index);

            switch ((*found).type)
            {
            case GL_FLOAT_VEC2_ARB:
                glVertexAttribPointer((*found).index, 2, GL_FLOAT, GL_FALSE, Vertex::AttributeDataSize, (void*)offset);
                offset += 2 * sizeof(GLfloat);
                break;
            case GL_FLOAT_VEC3_ARB:
                glVertexAttribPointer((*found).index, 3, GL_FLOAT, GL_FALSE, Vertex::AttributeDataSize, (void*)offset);
                offset += 3 * sizeof(GLfloat);
                break;
            case GL_FLOAT_VEC4_ARB:
                glVertexAttribPointer((*found).index, 4, GL_FLOAT, GL_FALSE, Vertex::AttributeDataSize, (void*)offset);
                offset += 4 * sizeof(GLfloat);
                break;
            default:
                break;
            }
        }
    }
}

void GraphicsPrivate::unbindMesh(const MeshRegistration &, const ProgramRegistration & programRegistration)
{
    for (const HashId & attributeName : Vertex::Attributes)
    {
        auto found = std::find_if(programRegistration.attributes.begin(), programRegistration.attributes.end(), [attributeName](const ProgramAttribute & attribute)
        {
            return attribute.name == attributeName;
        });
        if (found != programRegistration.attributes.end())
        {
            glDisableVertexAttribArray((*found).index);
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

GraphicsPrivate::GraphicsPrivate() 
    : window(nullptr)
    , glContext(nullptr)
    , glewError(0)
    , toFullscreen(false)
    , fullscreen(false)
{
    
}

void GraphicsPrivate::registerGraphics(IGraphics * whatever)
{
    whatever->d = this;
    if (auto program = dynamic_cast<GraphicsProgram *>(whatever))
    {
        ProgramRegistration programRegistration;
        registerProgram(program, programRegistration);
        programRegistrations.push_back(std::make_pair(program, programRegistration));
    }
    if (auto mesh = dynamic_cast<Mesh *>(whatever))
    {
        const MeshFile & fileCache = load(mesh->filename);
        const MeshRegistration * meshRegistration = &registrationFor(meshRegistrations, mesh);
        if (meshRegistration->definitionId == HashId())
        {
            const MeshInstance & instance = fileCache.instances[0];
            mesh->name = instance.instanceId;
            mesh->instanceId = instance.instanceId;
            mesh->definitionId = instance.definitionId;
            mesh->filename = fileCache.filename.str;
            mesh->scale *= instance.scale;
            mesh->position += instance.position;
            mesh->orientation = linalg::qmul(instance.orientation, mesh->orientation);
            meshRegistration = &registrationFor(meshRegistrations, mesh);
        }
        auto definition = std::find_if(fileCache.definitions.begin(), fileCache.definitions.end(), [mesh](const auto & def){ return def.definitionId == mesh->definitionId; });
        if (definition != fileCache.definitions.end())
        {
            if (definition->doubleSided) mesh->renderFlags &= ~Render::BackFaceCulling;
        }
        auto instance = std::find_if(fileCache.instances.begin(), fileCache.instances.end(), [mesh](const auto & inst){ return inst.instanceId == mesh->instanceId; });
        if (instance != fileCache.instances.end())
        {
            auto material = std::find_if(fileCache.materials.begin(), fileCache.materials.end(), [instance](const MeshMaterial & material) { return material.name == instance->materialId; });
            if (material != fileCache.materials.end())
            {
                for (auto value : material->values)
                {
                    auto uniform = GraphicsUniformFactory::makeUniform(value.name, enumForVectorSize(value.data.size()));
                    uniform->setData(value.data.data());
                    if (auto sampler = dynamic_cast<GraphicsUniformSampler2D *>(uniform))
                    {
                        sampler->filename = value.filename;
                    }
                    mesh->add(uniform);
                }
            }
        }
        const_cast<MeshRegistration *>(meshRegistration)->referenceCount += 1;
    }
}

void GraphicsPrivate::unregisterGraphics(IGraphics * whatever)
{
    if (auto program = dynamic_cast<GraphicsProgram *>(whatever))
    {
        auto found = std::find_if(programRegistrations.begin(), programRegistrations.end(), [program](const std::pair<GraphicsProgram*, ProgramRegistration> & pair)
        {
            return pair.first == program;
        });
        if (found != programRegistrations.end())
        {
            unregisterProgram(found->second);
            programRegistrations.erase(found);
        }
    }
    if (auto mesh = dynamic_cast<Mesh *>(whatever))
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
    }
    whatever->d = nullptr;
}

void GraphicsPrivate::bindGraphics(IGraphics * whatever)
{
    if (auto program = dynamic_cast<GraphicsProgram *>(whatever))
    {
        bindProgram(registrationFor(programRegistrations, program));
    }
    if (auto mesh = dynamic_cast<Mesh *>(whatever))
    {
        bindMesh(registrationFor(meshRegistrations, mesh), registrationFor(programRegistrations, mesh->program));
    }
    whatever->bound = true;
}

void GraphicsPrivate::unbindGraphics(IGraphics * whatever)
{
    if (auto program = dynamic_cast<GraphicsProgram *>(whatever))
    {   
        unbindProgram(registrationFor(programRegistrations, program));
    }
    if (auto mesh = dynamic_cast<Mesh *>(whatever))
    {   
        unbindMesh(registrationFor(meshRegistrations, mesh), registrationFor(programRegistrations, mesh->program));
    }
    whatever->bound = false;
}

void GraphicsPrivate::setProgramUniforms(Mesh * mesh)
{
    if (!mesh->program->bound)
    {
        return;
    }

    const ProgramRegistration & programRegistration = registrationFor(programRegistrations, mesh->program);
    for (const ProgramUniform & programUniform : programRegistration.uniforms)
    {
        if (auto * uniform = mesh->uniform(programUniform.name))
        {
            setUniform(textureRegistrations, mesh, uniform, programUniform);
        }
    }
}

void GraphicsPrivate::setProgramUniform(GraphicsProgram * program, IGraphicsUniform * uniform)
{
    if (!program->bound)
    {
        return;
    }

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

void GraphicsPrivate::unsetProgramUniform(GraphicsProgram *program, IGraphicsUniform * uniform)
{
    if (!program->bound)
    {
        return;
    }

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

void GraphicsPrivate::draw(GraphicsProgram *, Mesh * mesh)
{
    mesh->bind();
    glDrawElements(GL_TRIANGLES, registrationFor(meshRegistrations, mesh).indexBufferSize, GL_UNSIGNED_INT, 0);
    mesh->unbind();
}

const MeshFile& GraphicsPrivate::load(const std::string &filename)
{
    auto foundCache = std::find_if(meshCache.begin(), meshCache.end(), [filename](const MeshFile &fileCache)
    {
        return fileCache.filename == filename;
    });
    if (foundCache == meshCache.end())
    {
        MeshFile fileCache;
        if (MeshLoader::load(filename, fileCache))
        {
            for (const auto & definition : fileCache.definitions)
            {
                MeshRegistration meshRegistration;
                registerMesh(definition, meshRegistration);
                meshRegistrations.push_back(meshRegistration);
            }
            for (const auto & material : fileCache.materials)
            {
                for (auto value : material.values)
                {
                    if (enumForVectorSize(value.data.size()) == GL_SAMPLER_2D_ARB && !value.filename.empty())
                    {
                        TextureRegistration textureRegistration = TextureRegistration(value.filename);
                        texture(&textureRegistration.buffer, sibling(value.filename, fileCache.filename.str));
                        textureRegistrations.push_back(textureRegistration);
                    }
                }
            }

            meshCache.push_back(fileCache);
            return meshCache.back();
        }
        else
        {
            static const MeshFile Invalid;
            return Invalid;
        }
    }
    else
    {
        return *foundCache;
    }
}
