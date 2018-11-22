#include "MeshLoader.h"
#include "MeshLoaderPrivate.h"

#include <collada.h>
#include <loose.h>
#include <linalg.h>
#include <functional>

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

const std::array<Attribute, 5> Vertex::Attributes =
{ Attribute{"vertexPosition",  offsetof(struct Vertex, position)},
  Attribute{"vertexTexcoord",  offsetof(struct Vertex, texcoord)},
  Attribute{"vertexNormal",    offsetof(struct Vertex, normal)},
  Attribute{"vertexTangent",   offsetof(struct Vertex, tangent)},
  Attribute{"vertexBitangent", offsetof(struct Vertex, bitangent)} };
const Attribute &Vertex::describe(HashId attribute)
{
    return *std::find_if(Attributes.begin(), Attributes.end(), [attribute](const Attribute & attr)
    {
        return attr.name == attribute;
    });
}

const MeshMaterial MeshMaterial::kDefault = MeshMaterial(HashId(),
                                       {MeshMaterialValue("uniformAmbient",  conv::stofa("0.000000 0.000000 0.000000")),
                                        MeshMaterialValue("uniformDiffuse",  conv::stofa("0.640000 0.640000 0.640000")),
                                        MeshMaterialValue("uniformSpecular", conv::stofa("0.500000 0.500000 0.500000")),
                                        MeshMaterialValue("uniformEmissive", conv::stofa("0.000000 0.000000 0.000000"))});

MeshFile MeshFile::kDefault(const std::string & name, size_t v)
{
    MeshFile meshFile;
    meshFile.filename = name;
    meshFile.materials.push_back(MeshMaterial::kDefault);

    MeshDefinition definition(name, v);
    meshFile.definitions.push_back(definition);

    MeshInstance instance(name, definition);
    instance.materialId = MeshMaterial::kDefault.name;
    meshFile.instances.push_back(instance);

    return meshFile;
}

bool MeshLoader::load(const std::string &filename, MeshFile &meshFile)
{
    DAE::Data data;
    if (DAE::parse(filename, data, DAE::Graphics))
    {
        meshFile.filename = data.filename;
        meshFile.materials.push_back(MeshMaterial::kDefault);
        for (const auto & libraryMaterial : data.materials)
        {
            auto effect = std::find_if(data.effects.begin(), data.effects.end(), [libraryMaterial](const DAE::Effect & effect){ return effect.id == libraryMaterial.effect.url; });
            if (effect != data.effects.end())
            {
                MeshMaterial material;
                material.name = libraryMaterial.id;
                for (const auto & value : (*effect).values)
                {
                    MeshMaterialValue v = value;
                    auto image = std::find_if(data.images.begin(), data.images.end(), [v](const DAE::Image & image){ return image.id == v.texture; });
                    if (image != data.images.end()) { v.texture = (*image).initFrom; }
                    material.values.push_back(v);
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
            if (libraryNode.geometry)
            {
                auto definition = std::find_if(meshFile.definitions.begin(), meshFile.definitions.end(), [libraryNode](const MeshDefinition & definition){ return definition.definitionId == libraryNode.geometry->url; });

                MeshInstance instance;
                instance.instanceId = libraryNode.id;
                instance.definitionId = (*definition).definitionId;
                if (libraryNode.geometry->material != HashId())
                    instance.materialId = libraryNode.geometry->material;
                else
                    instance.materialId = MeshMaterial::kDefault.name;
                float3x3 rot = identity;
                linalg::split(libraryNode.transform, instance.position, instance.scale, rot);
                rot = transpose(rot);
                instance.orientation = rotation_quat(rot);
                meshFile.instances.push_back(instance);
            }
        }
        for (auto & material : meshFile.materials)
            material.repeatTexcoords = true;

        return !meshFile.definitions.empty() && !meshFile.instances.empty();
    }
    return false;
}
