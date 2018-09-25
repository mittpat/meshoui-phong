#include "MeshLoader.h"
#include "MeshLoaderPrivate.h"

#include "loose.h"
#include <linalg.h>
#include <experimental/filesystem>
#include <functional>
#include <map>
#include <pugixml.hpp>

using namespace linalg;
using namespace linalg::aliases;
namespace std { namespace filesystem = experimental::filesystem; }

const std::array<Attribute, 5> Vertex::Attributes =
{ Attribute{"vertexPosition",  3},
  Attribute{"vertexTexcoord",  2},
  Attribute{"vertexNormal",    3},
  Attribute{"vertexTangent",   3},
  Attribute{"vertexBitangent", 3} };
const size_t Vertex::AttributeDataSize = sizeof(Vertex);

const MeshMaterial MeshMaterial::kDefault = MeshMaterial(HashId(),
                                       {MeshMaterialValue("uniformAmbient",  conv::stofa("0.000000 0.000000 0.000000")),
                                        MeshMaterialValue("uniformDiffuse",  conv::stofa("0.640000 0.640000 0.640000")),
                                        MeshMaterialValue("uniformSpecular", conv::stofa("0.500000 0.500000 0.500000")),
                                        MeshMaterialValue("uniformEmissive", conv::stofa("0.000000 0.000000 0.000000"))});

namespace
{
    struct Reference
    {
        HashId id;
        std::string source;
    };

    struct LibraryImage
    {
        HashId id;
        std::string source;
    };

    struct LibraryEffect
    {
        HashId id;
        std::vector<Reference> surfaces;
        std::vector<Reference> samplers;
        std::vector<MeshMaterialValue> values;
        std::string solve(HashId v) const;
    };
    inline std::string LibraryEffect::solve(HashId v) const
    {
        auto ref = std::find_if(samplers.begin(), samplers.end(), [&](const Reference & ref) { return ref.id == v; });
        if (ref == samplers.end())
            return v.str;
        v = (*ref).source;
        ref = std::find_if(surfaces.begin(), surfaces.end(), [&](const Reference & ref) { return ref.id == v; });
        if (ref == surfaces.end())
            return v.str;
        return (*ref).source;
    }

    struct LibraryMaterial
    {
        HashId id;
        std::string source;
    };

    struct LibraryGeometry
    {
        Geometry geometry;
        Attributes attributes;
    };

    struct AttributeInput
    {
        AttributeInput();
        HashId id;
        std::string source;
        unsigned int offset;
    };
    inline AttributeInput::AttributeInput() : offset(0) {}

    struct EffectWalker
        : public pugi::xml_tree_walker
    {
        EffectWalker(LibraryEffect * e);
        virtual bool for_each(pugi::xml_node& node) override;
        LibraryEffect * effect;
    };
    inline EffectWalker::EffectWalker(LibraryEffect *e) : effect(e) {}
    inline bool EffectWalker::for_each(pugi::xml_node &node)
    {
        if (strcmp(node.name(), "newparam") == 0)
        {
            Reference reference;
            reference.id = node.attribute("sid").as_string();
            if (pugi::xml_node surface = node.child("surface"))
            {
                reference.source = surface.child_value("init_from");
                effect->surfaces.push_back(reference);
            }
            if (pugi::xml_node sampler2D = node.child("sampler2D"))
            {
                if ((reference.source = sampler2D.child_value("source")).empty())
                    reference.source = remainder(sampler2D.child("instance_image").attribute("url").as_string(), "#");
                effect->samplers.push_back(reference);
            }
        }
        return true;
    }

    struct PhongWalker
        : public pugi::xml_tree_walker
    {
        PhongWalker(LibraryEffect * e);
        virtual bool for_each(pugi::xml_node& node) override;
        LibraryEffect * effect;
    };
    inline PhongWalker::PhongWalker(LibraryEffect *e) : effect(e) {}
    inline bool PhongWalker::for_each(pugi::xml_node &node)
    {
        if (strcmp(node.name(), "ambient") == 0)
        {
            if (pugi::xml_node value = node.child("color"))   { effect->values.push_back(MeshMaterialValue("uniformAmbient", conv::stofa(value.child_value()))); }
            if (pugi::xml_node value = node.child("texture")) { effect->values.push_back(MeshMaterialValue("uniformTextureAmbient", effect->solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(node.name(), "diffuse") == 0)
        {
            if (pugi::xml_node value = node.child("color"))   { effect->values.push_back(MeshMaterialValue("uniformDiffuse", conv::stofa(value.child_value()))); }
            if (pugi::xml_node value = node.child("texture")) { effect->values.push_back(MeshMaterialValue("uniformTextureDiffuse", effect->solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(node.name(), "specular") == 0)
        {
            if (pugi::xml_node value = node.child("color"))   { effect->values.push_back(MeshMaterialValue("uniformSpecular", conv::stofa(value.child_value()))); }
            if (pugi::xml_node value = node.child("texture")) { effect->values.push_back(MeshMaterialValue("uniformTextureSpecular", effect->solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(node.name(), "emission") == 0)
        {
            if (pugi::xml_node value = node.child("color"))   { effect->values.push_back(MeshMaterialValue("uniformEmissive", conv::stofa(value.child_value()))); }
            if (pugi::xml_node value = node.child("texture")) { effect->values.push_back(MeshMaterialValue("uniformTextureEmissive", effect->solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(node.name(), "bump") == 0)
        {
            if (pugi::xml_node value = node.child("texture")) { effect->values.push_back(MeshMaterialValue("uniformTextureNormal", effect->solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(node.name(), "shininess") == 0)
        {
        // N/A
        }
        return true;
    }

    struct InstanceMaterialWalker
        : public pugi::xml_tree_walker
    {
        virtual bool for_each(pugi::xml_node& node) override;
        std::string materialId;
    };
    inline bool InstanceMaterialWalker::for_each(pugi::xml_node &node)
    {
        if (strcmp(node.name(), "instance_material") == 0) { materialId = remainder(node.attribute("target").as_string(), "#"); }
        return true;
    }
}

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

bool MeshLoader::load(const std::string & filename, MeshFile &meshFile)
{
    if (MeshLoader::loadDae(filename, meshFile))
        return true;

    return false;
}

namespace
{
    void processSceneElement(MeshFile & fileCache, pugi::xml_node elem_scene, float4x4 transform, const std::string & upAxis = "Z_UP")
    {
        for (pugi::xml_node property : elem_scene)
        {
            if (strcmp(property.name(), "matrix") == 0)
            {
                // Collada is row major
                // linalg.h is column major
                auto m44 = conv::stofa(property.child_value());
                transform = mul(transform, float4x4(float4(m44[0], m44[4], m44[8], m44[12]),
                                                    float4(m44[1], m44[5], m44[9], m44[13]),
                                                    float4(m44[2], m44[6], m44[10], m44[14]),
                                                    float4(m44[3], m44[7], m44[11], m44[15])));
            }
            else if (strcmp(property.name(), "translate") == 0)
            {
                transform = mul(transform, translation_matrix(conv::stof3(property.child_value())));
            }
            else if (strcmp(property.name(), "rotate") == 0)
            {
                auto v4 = float4(conv::stofa(property.child_value()).data());
                transform = mul(transform, rotation_matrix(rotation_quat(float3(v4.x, v4.y, v4.z), degreesToRadians(v4.w))));
            }
            else if (strcmp(property.name(), "scale") == 0)
            {
                transform = mul(transform, scaling_matrix(conv::stof3(property.child_value())));
            }
            else
            {
                processSceneElement(fileCache, property, transform, upAxis);
            }
        }
        if (upAxis == "Z_UP")
        {
            transform = mul(float4x4(float4(1, 0, 0, 0),
                                     float4(0, 0,-1, 0),
                                     float4(0, 1, 0, 0),
                                     float4(0, 0, 0, 1)), transform);
        }
        else if (upAxis == "X_UP")
        {
            transform = mul(float4x4(float4( 0, 1, 0, 0),
                                     float4(-1, 0, 0, 0),
                                     float4( 0, 0, 1, 0),
                                     float4( 0, 0, 0, 1)), transform);
        }
        if (auto instance_geometry = elem_scene.child("instance_geometry"))
        {
            HashId reference = HashId(remainder(instance_geometry.attribute("url").as_string(), "#"), fileCache.filename);
            auto definition = std::find_if(fileCache.definitions.begin(), fileCache.definitions.end(), [reference](const MeshDefinition & definition){ return definition.definitionId == reference; });

            MeshInstance instance;
            instance.instanceId = elem_scene.attribute("id").as_string();
            instance.definitionId = (*definition).definitionId;
            InstanceMaterialWalker instanceMaterial;
            instance_geometry.traverse(instanceMaterial);
            if (!instanceMaterial.materialId.empty())
                instance.materialId = instanceMaterial.materialId;
            else
                instance.materialId = MeshMaterial::kDefault.name;
            float3x3 rot = identity;
            linalg::split(transform, instance.position, instance.scale, rot);
            rot = transpose(rot);
            instance.orientation = rotation_quat(rot);
            fileCache.instances.push_back(instance);
        }
    }
}

bool MeshLoader::loadDae(const std::string &filename, MeshFile &meshFile)
{
    if (std::filesystem::path(filename).extension() != ".dae")
        return false;

    if (!std::filesystem::exists(filename))
        return false;

    meshFile.filename = filename;
    printf("Loading '%s'\n", std::filesystem::absolute(filename).c_str());

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.c_str());
    if (result.status != pugi::status_ok)
    {
        return false;
    }

    auto root = doc.child("COLLADA");
    auto version = root.attribute("version");
    std::string sversion = version.as_string();
    printf("COLLADA version '%s'\n", sversion.c_str());

    meshFile.materials.push_back(MeshMaterial::kDefault);

    std::vector<LibraryImage> libraryImages;
    for (pugi::xml_node elem_image : root.child("library_images"))
    {
        LibraryImage libraryImage;
        libraryImage.id = elem_image.attribute("id").as_string();
        if ((libraryImage.source = elem_image.child("init_from").child_value("ref")).empty())
            libraryImage.source = elem_image.child_value("init_from");
        libraryImages.push_back(libraryImage);
    }
    std::vector<LibraryEffect> libraryEffects;
    for (pugi::xml_node elem_effect : root.child("library_effects"))
    {
        LibraryEffect libraryEffect;
        libraryEffect.id = elem_effect.attribute("id").as_string();
        EffectWalker effectWalker(&libraryEffect);
        elem_effect.traverse(effectWalker);
        PhongWalker phongWalker(&libraryEffect);
        elem_effect.traverse(phongWalker);
        libraryEffects.push_back(libraryEffect);
    }
    for (pugi::xml_node elem_material : root.child("library_materials"))
    {
        HashId effectName = remainder(elem_material.child("instance_effect").attribute("url").value(), "#");
        auto effect = std::find_if(libraryEffects.begin(), libraryEffects.end(), [effectName](const LibraryEffect & effect){ return effect.id == effectName; });
        if (effect != libraryEffects.end())
        {
            MeshMaterial libraryMaterial;
            libraryMaterial.name = elem_material.attribute("id").as_string();
            for (const auto & value : (*effect).values)
            {
                MeshMaterialValue v = value;
                auto image = std::find_if(libraryImages.begin(), libraryImages.end(), [v](const LibraryImage & image){ return image.id == v.filename; });
                if (image != libraryImages.end()) { v.filename = (*image).source; }
                libraryMaterial.values.push_back(v);
            }
            meshFile.materials.push_back(libraryMaterial);
        }
    }
    std::vector<LibraryGeometry> libraryGeometries;
    for (pugi::xml_node elem_geometry : root.child("library_geometries"))
    {
        LibraryGeometry libraryGeometry;
        libraryGeometry.geometry.bbox = AABB();
        libraryGeometry.geometry.id = HashId(elem_geometry.attribute("id").as_string(), filename);

        printf("Loading '%s'\n", libraryGeometry.geometry.id.str.c_str());
        if (pugi::xml_node mesh = elem_geometry.child("mesh"))
        {
            std::vector<AttributeInput> attributeInputs;
            pugi::xml_node polylist = mesh.child("polylist");
            if (!polylist)
            {
                polylist = mesh.child("triangles");
            }
            for (pugi::xml_node elem : polylist)
            {
                if (strcmp(elem.name(), "input") == 0)
                {
                    AttributeInput input;
                    input.id = elem.attribute("semantic").as_string();
                    input.source = remainder(elem.attribute("source").as_string(), "#");
                    input.offset = elem.attribute("offset").as_uint();
                    attributeInputs.push_back(input);
                }
            }
            if (pugi::xml_node vertices = mesh.child("vertices"))
            {
                HashId id = vertices.attribute("id").as_string();
                auto input = std::find_if(attributeInputs.begin(), attributeInputs.end(), [id](const AttributeInput & input) { return id == input.source; });
                if (input != attributeInputs.end())
                {
                    (*input).source = remainder(vertices.child("input").attribute("source").as_string(), "#");
                }
            }
            static const HashId kPosition = "POSITION";
            static const HashId kVertex = "VERTEX";
            static const HashId kNormal = "NORMAL";
            static const HashId kTexcoord = "TEXCOORD";
            for (pugi::xml_node source : mesh)
            {
                if (strcmp(source.name(), "source") == 0)
                {
                    HashId id = source.attribute("id").as_string();
                    auto input = std::find_if(attributeInputs.begin(), attributeInputs.end(), [id](const AttributeInput & input) { return id == input.source; });
                    if (input != attributeInputs.end())
                    {
                        if (kPosition == (*input).id || kVertex == (*input).id)
                        {
                            libraryGeometry.attributes.vertices = conv::stof3a(source.child_value("float_array"));
                            for (float3 vertex : libraryGeometry.attributes.vertices)
                                libraryGeometry.geometry.bbox.extend(vertex);
                        }
                        else if (kNormal == (*input).id)
                        {
                            libraryGeometry.attributes.normals = conv::stof3a(source.child_value("float_array"));
                        }
                        else if (kTexcoord == (*input).id)
                        {
                            libraryGeometry.attributes.texcoords = conv::stof2a(source.child_value("float_array"));
                        }
                    }
                }
            }
            if (polylist)
            {
                std::vector<unsigned int> polygons(polylist.attribute("count").as_uint(), 3);
                if (auto data = polylist.child("vcount"))
                {
                    polygons = conv::stouia(data.child_value());
                }
                if (auto data = polylist.child("p"))
                {
                    unsigned int max = 0;
                    std::map<size_t, unsigned int> sourceIndexes;
                    for (const auto & input : attributeInputs)
                    {
                        sourceIndexes[input.id] = input.offset;
                        max = std::max(max, input.offset);
                    }
                    max += 1;
                    std::vector<unsigned int> indexes = conv::stouia(data.child_value());
                    size_t i = 0;
                    for (size_t j = 0; j < polygons.size(); ++j)
                    {
                        auto vcount = polygons[j];
                        for (size_t k = 2; k < vcount; ++k)
                        {
                            Triangle face;
                            if (sourceIndexes.find(kVertex) != sourceIndexes.end())
                            {
                                face.vertices.x = 1+indexes[i+sourceIndexes[kVertex]+(0  )*max];
                                face.vertices.y = 1+indexes[i+sourceIndexes[kVertex]+(k-1)*max];
                                face.vertices.z = 1+indexes[i+sourceIndexes[kVertex]+(k  )*max];
                            }
                            if (sourceIndexes.find(kNormal) != sourceIndexes.end())
                            {
                                face.normals.x = 1+indexes[i+sourceIndexes[kNormal]+(0  )*max];
                                face.normals.y = 1+indexes[i+sourceIndexes[kNormal]+(k-1)*max];
                                face.normals.z = 1+indexes[i+sourceIndexes[kNormal]+(k  )*max];
                            }
                            if (sourceIndexes.find(kTexcoord) != sourceIndexes.end())
                            {
                                face.texcoords.x = 1+indexes[i+sourceIndexes[kTexcoord]+(0  )*max];
                                face.texcoords.y = 1+indexes[i+sourceIndexes[kTexcoord]+(k-1)*max];
                                face.texcoords.z = 1+indexes[i+sourceIndexes[kTexcoord]+(k  )*max];
                            }
                            libraryGeometry.geometry.triangles.push_back(face);
                        }
                        i += max * vcount;
                    }
                }
            }
        }
        libraryGeometry.geometry.doubleSided = conv::stoui(elem_geometry.child("extra").child("technique").child_value("double_sided"));
        libraryGeometries.push_back(libraryGeometry);
    }
    for (const auto & libraryGeometry : libraryGeometries)
    {
        MeshDefinition definition;
        buildGeometry(definition, libraryGeometry.attributes, libraryGeometry.geometry);
        meshFile.definitions.push_back(definition);
    }
    std::string upAxis = root.child("asset").child_value("up_axis");
    for (pugi::xml_node elem_scene : root.child("library_nodes"))
    {
        processSceneElement(meshFile, elem_scene, identity, upAxis);
    }
    for (pugi::xml_node elem_scene : root.child("library_visual_scenes").child("visual_scene"))
    {
        processSceneElement(meshFile, elem_scene, identity, upAxis);
    }

    for (auto & material : meshFile.materials)
        material.repeatTexcoords = true;

    fflush(stdout);

    return !meshFile.definitions.empty() && !meshFile.instances.empty();
}
