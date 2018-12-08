#include "MeshLoader.h"
#include "MeshLoaderPrivate.h"

#include <collada.h>
#include <loose.h>
#include <linalg.h>
#include <functional>

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
    instance.materialId = meshFile.materials[0].name;
    meshFile.instances.push_back(instance);

    return meshFile;
}

bool MeshLoader::load(const std::string &filename, MeshFile &meshFile)
{
    DAE::Data data;
    if (DAE::parse(filename, data, DAE::Graphics))
    {
        meshFile.filename = data.filename;
        meshFile.materials.push_back(MeshMaterial());
        meshFile.materials[0].textureDiffuse = meshFile.materials[0].textureSpecular = meshFile.materials[0].textureEmissive = meshFile.materials[0].textureNormal = "brick.dds";
        for (const auto & libraryMaterial : data.materials)
        {
            auto effect = std::find_if(data.effects.begin(), data.effects.end(), [libraryMaterial](const DAE::Effect & effect){ return effect.id == libraryMaterial.effect.url; });
            if (effect != data.effects.end())
            {
                MeshMaterial material;
                material.name = libraryMaterial.id;
                material.textureDiffuse = material.textureSpecular = material.textureEmissive = material.textureNormal = "brick.dds";
                for (const auto & value : (*effect).values)
                {
                    DAE::Effect::Value v = value;
                    auto image = std::find_if(data.images.begin(), data.images.end(), [v](const DAE::Image & image){ return image.id == v.texture; });
                    if (image != data.images.end()) { v.texture = (*image).initFrom; }

                    if (v.sid == "uniformAmbient") { material.ambient = (float3&)v.data[0]; }
                    else if (v.sid == "uniformDiffuse") { material.diffuse = (float3&)v.data[0]; }
                    else if (v.sid == "uniformTextureDiffuse") { material.textureDiffuse = v.texture; }
                    else if (v.sid == "uniformSpecular") { material.specular = (float3&)v.data[0]; }
                    else if (v.sid == "uniformTextureSpecular") { material.textureSpecular = v.texture; }
                    else if (v.sid == "uniformEmissive") { material.emissive = (float3&)v.data[0]; }
                    else if (v.sid == "uniformTextureEmissive") { material.textureEmissive = v.texture; }
                    else if (v.sid == "uniformTextureNormal") { material.textureNormal = v.texture; }
                }
                meshFile.materials.push_back(material);
            }
        }
        for (const auto & libraryGeometry : data.geometries)
        {
            MeshDefinition definition;
            definition.definitionId = libraryGeometry.id;
            definition.doubleSided = libraryGeometry.doubleSided;
            buildGeometry(definition, libraryGeometry.mesh);
            meshFile.definitions.push_back(definition);
        }
        for (const auto & libraryNode : data.nodes)
        {
            if (libraryNode.geometry.name != HashId())
            {
                auto definition = std::find_if(meshFile.definitions.begin(), meshFile.definitions.end(), [libraryNode](const MeshDefinition & definition){ return definition.definitionId == libraryNode.geometry.url; });

                MeshInstance instance;
                instance.instanceId = libraryNode.id;
                instance.definitionId = (*definition).definitionId;
                if (libraryNode.geometry.material != HashId())
                    instance.materialId = libraryNode.geometry.material;
                else
                    instance.materialId = meshFile.materials[0].name;
                float3x3 rot = identity;
                linalg::split(libraryNode.transform, instance.position, instance.scale, rot);
                rot = transpose(rot);
                instance.orientation = rotation_quat(rot);
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
