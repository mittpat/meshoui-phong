#include "Mesh.h"
#include "Program.h"
#include "RendererPrivate.h"
#include "TextureLoader.h"
#include "Uniform.h"

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <loose.h>
#include <algorithm>
#include <experimental/filesystem>
#include <fstream>

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

    void setUniform(const TextureRegistrations &, Mesh *, IUniform *, const ProgramUniform &)
    {
        //
    }

    bool registerShader(GLenum, const std::vector<std::string> &, const std::string &, GLuint &, std::string * const)
    {
        //

        return false;
    }
}

void RendererPrivate::unregisterProgram(const ProgramRegistration &)
{
    //
}

bool RendererPrivate::registerProgram(Program * program, ProgramRegistration & programRegistration)
{
    GLuint vertex, fragment;
    if (!registerShader(GL_VERTEX_SHADER, program->defines, program->vertexShaderSource, vertex, &program->lastError))
        return false;

    if (!registerShader(GL_FRAGMENT_SHADER, program->defines, program->fragmentShaderSource, fragment, &program->lastError))
        return false;

    //

    return false;
}

void RendererPrivate::bindProgram(const ProgramRegistration & programRegistration)
{
    //
}

void RendererPrivate::unbindProgram(const ProgramRegistration & programRegistration)
{
    //
}

void RendererPrivate::unregisterMesh(const MeshRegistration & meshRegistration)
{
    //
}

void RendererPrivate::registerMesh(const MeshDefinition & meshDefinition, MeshRegistration & meshRegistration)
{
    //

    meshRegistration.indexBufferSize = meshDefinition.indices.size();
    meshRegistration.vertexBufferSize = meshDefinition.vertices.size();
    meshRegistration.definitionId = meshDefinition.definitionId;
}

void RendererPrivate::bindMesh(const MeshRegistration & meshRegistration, const ProgramRegistration & programRegistration)
{
    //

    size_t offset = 0;
    for (const Attribute & attributeDef : Vertex::Attributes)
    {
        auto found = std::find_if(programRegistration.attributes.begin(), programRegistration.attributes.end(), [attributeDef](const ProgramAttribute & attribute)
        {
            return attribute.name == attributeDef.name;
        });
        if (found != programRegistration.attributes.end())
        {
            //
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
            //
        }
    }

    //
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
        //
    }
}

void RendererPrivate::draw(Program *, Mesh * mesh)
{
    //
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
