#include "MeshLoader.h"
#include "MeshLoaderPrivate.h"

#include <collada.h>
#include <linalg.h>
#include <functional>

#include <experimental/filesystem>

namespace std { namespace filesystem = experimental::filesystem; }
namespace
{
    std::string sibling(const std::string & path, const std::string & other)
    {
        std::filesystem::path parentpath(other);
        std::filesystem::path parentdirectory = parentpath.parent_path();
        return (parentdirectory / path).u8string();
    }
}

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

MeshFile MeshFile::kDefault(const std::string & name, size_t v)
{
    MeshFile meshFile;
    meshFile.filename = name;
    meshFile.materials.push_back(MeshMaterial());

    MeshDefinition definition(name, v);
    meshFile.definitions.push_back(definition);

    MeshInstance instance(name, definition);
    instance.materialId = meshFile.materials[0].materialId;
    meshFile.instances.push_back(instance);

    return meshFile;
}

bool MeshLoader::load(const std::string &filename, MeshFile &meshFile)
{
    if (filename == "builtin_shape_cube")
    {
        cube(filename, meshFile);
        return true;
    }

    DAE::Data data;
    if (DAE::parse(filename, data, DAE::Graphics))
    {
        meshFile.filename = data.filename;
        meshFile.materials.push_back(MeshMaterial());
        for (const auto & libraryMaterial : data.materials)
        {
            auto effect = std::find_if(data.effects.begin(), data.effects.end(), [libraryMaterial](const DAE::Effect & effect){ return effect.id == libraryMaterial.effect.url; });
            if (effect != data.effects.end())
            {
                MeshMaterial material;
                material.materialId = libraryMaterial.id;
                for (const auto & value : (*effect).values)
                {
                    DAE::Effect::Value v = value;
                    auto image = std::find_if(data.images.begin(), data.images.end(), [v](const DAE::Image & image){ return image.id == v.texture; });
                    if (image != data.images.end()) { v.texture = (*image).initFrom; }

                    if (v.sid == "uniformAmbient") { material.ambient = (float3&)v.data[0]; }
                    else if (v.sid == "uniformTextureAmbient") { material.textureAmbient = sibling(v.texture, meshFile.filename); }
                    else if (v.sid == "uniformDiffuse") { material.diffuse = (float3&)v.data[0]; }
                    else if (v.sid == "uniformTextureDiffuse") { material.textureDiffuse = sibling(v.texture, meshFile.filename); }
                    else if (v.sid == "uniformSpecular") { material.specular = (float3&)v.data[0]; }
                    else if (v.sid == "uniformTextureSpecular") { material.textureSpecular = sibling(v.texture, meshFile.filename); }
                    else if (v.sid == "uniformEmissive") { material.emissive = (float3&)v.data[0]; }
                    else if (v.sid == "uniformTextureEmissive") { material.textureEmissive = sibling(v.texture, meshFile.filename); }
                    else if (v.sid == "uniformTextureNormal") { material.textureNormal = sibling(v.texture, meshFile.filename); }
                }
                meshFile.materials.push_back(material);
            }
        }
        for (const auto & libraryGeometry : data.geometries)
        {
            MeshDefinition definition;
            definition.definitionId = HashId(libraryGeometry.id, filename);
            definition.doubleSided = libraryGeometry.doubleSided;
#if 0
            std::vector<linalg::aliases::float2> texcoords;
            for (auto & t : libraryGeometry.mesh.triangles)
            {
                {
                    float2 uv = libraryGeometry.mesh.texcoords[t.texcoords.x-1];
                    auto current = std::find(texcoords.begin(), texcoords.end(), uv);
                    if (current == texcoords.end())
                    {
                        texcoords.push_back(uv);
                        t.texcoords.x = texcoords.size();
                    }
                    else
                    {
                        t.texcoords.x = std::distance(texcoords.begin(), current)+1;
                    }
                }
                {
                    float2 uv = libraryGeometry.mesh.texcoords[t.texcoords.y-1];
                    auto current = std::find(texcoords.begin(), texcoords.end(), uv);
                    if (current == texcoords.end())
                    {
                        texcoords.push_back(uv);
                        t.texcoords.y = texcoords.size();
                    }
                    else
                    {
                        t.texcoords.y = std::distance(texcoords.begin(), current)+1;
                    }
                }
                {
                    float2 uv = libraryGeometry.mesh.texcoords[t.texcoords.z-1];
                    auto current = std::find(texcoords.begin(), texcoords.end(), uv);
                    if (current == texcoords.end())
                    {
                        texcoords.push_back(uv);
                        t.texcoords.z = texcoords.size();
                    }
                    else
                    {
                        t.texcoords.z = std::distance(texcoords.begin(), current)+1;
                    }
                }
            }
            libraryGeometry.mesh.texcoords = texcoords;
#endif
            buildGeometry(definition, libraryGeometry.mesh);
            meshFile.definitions.push_back(definition);
        }
        for (const auto & libraryNode : data.nodes)
        {
            if (!libraryNode.geometry.name.empty())
            {
                HashId definitionId(libraryNode.geometry.url, filename);
                auto definition = std::find_if(meshFile.definitions.begin(), meshFile.definitions.end(), [definitionId](const MeshDefinition & definition){ return definition.definitionId == definitionId; });

                MeshInstance instance;
                instance.instanceId = libraryNode.id;
                instance.definitionId = (*definition).definitionId;
                if (!libraryNode.geometry.material.empty())
                    instance.materialId = libraryNode.geometry.material;
                else
                    instance.materialId = meshFile.materials[0].materialId;
                instance.modelMatrix = (const float4x4&)libraryNode.transform;
                meshFile.instances.push_back(instance);
            }
        }
        /*
        for (auto & material : meshFile.materials)
            material.repeatTexcoords = true;
        */
        return !meshFile.definitions.empty() && !meshFile.instances.empty();
    }
    return false;
}

void MeshLoader::cube(const std::string &name, MeshFile &meshFile)
{
    meshFile.filename = name;
    meshFile.materials.resize(1);
    meshFile.materials[0].materialId = name + "_cube_mat";
    meshFile.materials[0].ambient = float3(0.25,0.25,0.25);
    meshFile.materials[0].textureDiffuse = "meshoui/resources/models/brick.png";
    meshFile.materials[0].textureNormal = "meshoui/resources/models/bricknormal.png";
    meshFile.definitions.resize(1);
    meshFile.definitions[0].definitionId = name + "_cube_def";
    meshFile.instances.resize(1);
    meshFile.instances[0].definitionId = meshFile.definitions[0].definitionId;
    meshFile.instances[0].materialId = meshFile.materials[0].materialId;
    meshFile.instances[0].instanceId = name + "_cube_inst";

    DAE::Mesh mesh;
    mesh.vertices = {{-1.0, -1.0, -1.0},
                     {-1.0, -1.0,  1.0},
                     {-1.0,  1.0, -1.0},
                     {-1.0,  1.0,  1.0},
                     { 1.0, -1.0, -1.0},
                     { 1.0, -1.0,  1.0},
                     { 1.0,  1.0, -1.0},
                     { 1.0,  1.0,  1.0}};
    mesh.texcoords = {{1,0},
                      {0,2},
                      {0,0},
                      {1,2}};
    mesh.triangles = {{{2,3,1}, {1,2,3}, {0,0,0}},
                      {{4,7,3}, {1,2,3}, {0,0,0}},
                      {{8,5,7}, {1,2,3}, {0,0,0}},
                      {{6,1,5}, {1,2,3}, {0,0,0}},
                      {{7,1,3}, {1,2,3}, {0,0,0}},
                      {{4,6,8}, {1,2,3}, {0,0,0}},
                      {{2,4,3}, {1,4,2}, {0,0,0}},
                      {{4,8,7}, {1,4,2}, {0,0,0}},
                      {{8,6,5}, {1,4,2}, {0,0,0}},
                      {{6,2,1}, {1,4,2}, {0,0,0}},
                      {{7,5,1}, {1,4,2}, {0,0,0}},
                      {{4,2,6}, {1,4,2}, {0,0,0}}};
    buildGeometry(meshFile.definitions[0], mesh, true);
}
