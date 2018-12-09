#include "collada.h"
#include "loose.h"
#include <experimental/filesystem>
#include <functional>
#include <map>
#include <pugixml.hpp>

using namespace linalg;
using namespace linalg::aliases;
namespace std { namespace filesystem = experimental::filesystem; }

inline std::string DAE::Effect::solve(HashId v) const
{
    auto sampler = std::find_if(samplers.begin(), samplers.end(), [&](const Sampler & ref) { return ref.sid == v; });
    if (sampler == samplers.end())
        return v.str;
    v = (*sampler).source;
    auto surface = std::find_if(surfaces.begin(), surfaces.end(), [&](const Surface & ref) { return ref.sid == v; });
    if (surface == surfaces.end())
        return v.str;
    return (*surface).initFrom;
}

void DAE::parse_effect_profile_phong(pugi::xml_node branch, DAE::Effect & effect)
{
    for (pugi::xml_node phong_value : branch)
    {
        if (strcmp(phong_value.name(), "ambient") == 0)
        {
            if (pugi::xml_node value = phong_value.child("color"))   { effect.values.push_back(DAE::Effect::Value("uniformAmbient", conv::stofa(value.child_value()))); }
            if (pugi::xml_node value = phong_value.child("texture")) { effect.values.push_back(DAE::Effect::Value("uniformTextureAmbient", effect.solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(phong_value.name(), "diffuse") == 0)
        {
            if (pugi::xml_node value = phong_value.child("color"))   { effect.values.push_back(DAE::Effect::Value("uniformDiffuse", conv::stofa(value.child_value()))); }
            if (pugi::xml_node value = phong_value.child("texture")) { effect.values.push_back(DAE::Effect::Value("uniformTextureDiffuse", effect.solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(phong_value.name(), "specular") == 0)
        {
            if (pugi::xml_node value = phong_value.child("color"))   { effect.values.push_back(DAE::Effect::Value("uniformSpecular", conv::stofa(value.child_value()))); }
            if (pugi::xml_node value = phong_value.child("texture")) { effect.values.push_back(DAE::Effect::Value("uniformTextureSpecular", effect.solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(phong_value.name(), "emission") == 0)
        {
            if (pugi::xml_node value = phong_value.child("color"))   { effect.values.push_back(DAE::Effect::Value("uniformEmissive", conv::stofa(value.child_value()))); }
            if (pugi::xml_node value = phong_value.child("texture")) { effect.values.push_back(DAE::Effect::Value("uniformTextureEmissive", effect.solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(phong_value.name(), "bump") == 0)
        {
            if (pugi::xml_node value = phong_value.child("texture")) { effect.values.push_back(DAE::Effect::Value("uniformTextureNormal", effect.solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(phong_value.name(), "shininess") == 0)
        {
        // N/A
        }
    }
}

void DAE::parse_effect_profile(pugi::xml_node branch, DAE::Effect & effect)
{
    for (pugi::xml_node profile_value : branch)
    {
        if (strcmp(profile_value.name(), "newparam") == 0)
        {
            if (pugi::xml_node surface = profile_value.child("surface"))
            {
                DAE::Surface reference;
                reference.sid = profile_value.attribute("sid").as_string();
                reference.initFrom = surface.child_value("init_from");
                effect.surfaces.push_back(reference);
            }
            else if (pugi::xml_node sampler2D = profile_value.child("sampler2D"))
            {
                DAE::Sampler reference;
                reference.sid = profile_value.attribute("sid").as_string();
                if ((reference.source = sampler2D.child_value("source")).empty())
                    reference.source = remainder(sampler2D.child("instance_image").attribute("url").as_string(), "#");
                effect.samplers.push_back(reference);
            }
        }
        else if (strcmp(profile_value.name(), "technique") == 0)
        {
            parse_effect_profile_phong(profile_value.child("phong"), effect);
            if (pugi::xml_node extra = profile_value.child("extra"))
            {
                parse_effect_profile_phong(extra.child("technique"), effect);
            }
        }
    }
}

void DAE::parse_geometry_mesh(pugi::xml_node branch, DAE::Geometry & geometry)
{
    struct Input
    {
        Input() : offset(0) {}
        HashId id;
        HashId source;
        unsigned int offset;
    };

    std::vector<Input> inputs;
    pugi::xml_node polylist = branch.child("polylist");
    if (!polylist)
    {
        polylist = branch.child("triangles");
    }
    for (pugi::xml_node elem : polylist)
    {
        if (strcmp(elem.name(), "input") == 0)
        {
            Input input;
            input.id = elem.attribute("semantic").as_string();
            input.source = remainder(elem.attribute("source").as_string(), "#");
            input.offset = elem.attribute("offset").as_uint();
            inputs.push_back(input);
        }
    }
    if (pugi::xml_node vertices = branch.child("vertices"))
    {
        HashId id = vertices.attribute("id").as_string();
        auto input = std::find_if(inputs.begin(), inputs.end(), [id](const Input & input) { return id == input.source; });
        if (input != inputs.end())
        {
            (*input).source = remainder(vertices.child("input").attribute("source").as_string(), "#");
        }
    }
    static const HashId kPosition = "POSITION";
    static const HashId kVertex = "VERTEX";
    static const HashId kNormal = "NORMAL";
    static const HashId kTexcoord = "TEXCOORD";
    for (pugi::xml_node source : branch)
    {
        if (strcmp(source.name(), "source") == 0)
        {
            HashId id = source.attribute("id").as_string();
            auto input = std::find_if(inputs.begin(), inputs.end(), [id](const Input & input) { return id == input.source; });
            if (input != inputs.end())
            {
                if (kPosition == (*input).id || kVertex == (*input).id)
                {
                    geometry.mesh.vertices = conv::stof3a(source.child_value("float_array"));
                    for (float3 vertex : geometry.mesh.vertices)
                        geometry.mesh.bbox.extend(vertex);
                }
                else if (kNormal == (*input).id)
                {
                    geometry.mesh.normals = conv::stof3a(source.child_value("float_array"));
                }
                else if (kTexcoord == (*input).id)
                {
                    geometry.mesh.texcoords = conv::stof2a(source.child_value("float_array"));
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
            for (const auto & input : inputs)
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
                    geometry.mesh.triangles.push_back(face);
                }
                i += max * vcount;
            }
        }
    }
}

void DAE::parse_library_images(pugi::xml_node branch, DAE::Data & data)
{
    for (pugi::xml_node library_image : branch)
    {
        DAE::Image image;
        image.id = library_image.attribute("id").as_string();
        if ((image.initFrom = library_image.child("init_from").child_value("ref")).empty())
            image.initFrom = library_image.child_value("init_from");
        data.images.push_back(image);
    }
}

void DAE::parse_library_effects(pugi::xml_node branch, DAE::Data & data)
{
    for (pugi::xml_node library_effect : branch)
    {
        DAE::Effect libraryEffect;
        libraryEffect.id = library_effect.attribute("id").as_string();
        parse_effect_profile(library_effect.child("profile_COMMON"), libraryEffect);
        data.effects.push_back(libraryEffect);
    }
}

void DAE::parse_library_materials(pugi::xml_node branch, DAE::Data & data)
{
    for (pugi::xml_node library_material : branch)
    {
        DAE::Material libraryMaterial;
        libraryMaterial.id = library_material.attribute("id").as_string();
        libraryMaterial.effect.url = remainder(library_material.child("instance_effect").attribute("url").value(), "#");
        data.materials.push_back(libraryMaterial);
    }
}

void DAE::parse_library_geometries(pugi::xml_node branch, DAE::Data & data)
{
    for (pugi::xml_node library_geometry : branch)
    {
        Geometry geometry;
        geometry.id = HashId(library_geometry.attribute("id").as_string(), data.filename);
        geometry.doubleSided = conv::stoui(library_geometry.child("extra").child("technique").child_value("double_sided"));
        parse_geometry_mesh(library_geometry.child("mesh"), geometry);
        data.geometries.push_back(geometry);
    }
}

void DAE::parse_library_physics_models(pugi::xml_node branch, DAE::Data &data)
{
    for (pugi::xml_node library_physics_model : branch)
    {
        DAE::PhysicsModel model;
        model.id = library_physics_model.attribute("id").as_string();
        for (pugi::xml_node rigid_body : library_physics_model)
        {
            DAE::RigidBody body;
            body.sid = rigid_body.attribute("sid").as_string();
            body.dynamic = strcmp(rigid_body.child("technique_common").child_value("dynamic"), "true") == 0;
            body.shape.halfExtents = conv::stof3(rigid_body.child("technique_common").child("shape").child("box").child_value("half_extents"));
            model.bodies.push_back(body);
        }
        data.models.push_back(model);
    }
}

void DAE::parse_library_physics_scenes(pugi::xml_node branch, DAE::Data &data)
{
    for (pugi::xml_node library_physics_scene : branch)
    {
        for (pugi::xml_node instance_physics_model : library_physics_scene)
        {
            if (strcmp(instance_physics_model.name(), "instance_physics_model") == 0)
            {
                DAE::InstancePhysicsModel model;
                model.sid = instance_physics_model.attribute("sid").as_string();
                model.url = remainder(instance_physics_model.attribute("url").as_string(), "#");
                model.parent = remainder(instance_physics_model.attribute("parent").as_string(), "#");
                data.instances.push_back(model);
            }
        }
    }
}

void DAE::parse_library_visual_scenes(pugi::xml_node branch, DAE::Data &data)
{
    // Nodes
    std::function<void(pugi::xml_node, DAE::Node&)> parser = [&parser, &data](pugi::xml_node currentVisualNode, DAE::Node & currentNode)
    {
        currentNode.id = currentVisualNode.attribute("id").as_string();
        for (pugi::xml_node visualNode : currentVisualNode)
        {
            if (strcmp(visualNode.name(), "instance_geometry") == 0)
            {
                currentNode.geometry = DAE::InstanceGeometry();
                currentNode.geometry.name = visualNode.attribute("name").as_string();
                currentNode.geometry.url = HashId(remainder(visualNode.attribute("url").as_string(), "#"), data.filename);
                if (auto bind_material = visualNode.child("bind_material"))
                    if (auto technique_common = bind_material.child("technique_common"))
                        if (auto instance_material = technique_common.child("instance_material"))
                            currentNode.geometry.material = remainder(instance_material.attribute("target").as_string(), "#");
            }
            else if (strcmp(visualNode.name(), "matrix") == 0)
            {
                // Collada is row major
                // linalg.h is column major
                auto m44 = conv::stofa(visualNode.child_value());
                currentNode.transform = mul(currentNode.transform,
                                            float4x4(float4(m44[0], m44[4], m44[8], m44[12]),
                                                     float4(m44[1], m44[5], m44[9], m44[13]),
                                                     float4(m44[2], m44[6], m44[10], m44[14]),
                                                     float4(m44[3], m44[7], m44[11], m44[15])));
            }
            else if (strcmp(visualNode.name(), "translate") == 0)
            {
                currentNode.transform = mul(currentNode.transform, translation_matrix(conv::stof3(visualNode.child_value())));
            }
            else if (strcmp(visualNode.name(), "rotate") == 0)
            {
                auto v4 = float4(conv::stofa(visualNode.child_value()).data());
                currentNode.transform = mul(currentNode.transform, rotation_matrix(rotation_quat(float3(v4.x, v4.y, v4.z), degreesToRadians(v4.w))));
            }
            else if (strcmp(visualNode.name(), "scale") == 0)
            {
                currentNode.transform = mul(currentNode.transform, scaling_matrix(conv::stof3(visualNode.child_value())));
            }
        }

        for (pugi::xml_node visualNode : currentVisualNode)
        {
            if (strcmp(visualNode.name(), "node") == 0)
            {
                DAE::Node node;
                node.transform = currentNode.transform;
                parser(visualNode, node);
                data.nodes.push_back(node);
            }
        }

        if (data.upAxis == "Z_UP")
        {
            currentNode.transform = mul(float4x4(float4(1, 0, 0, 0),
                                                 float4(0, 0,-1, 0),
                                                 float4(0, 1, 0, 0),
                                                 float4(0, 0, 0, 1)), currentNode.transform);
        }
        else if (data.upAxis == "X_UP")
        {
            currentNode.transform = mul(float4x4(float4( 0, 1, 0, 0),
                                                 float4(-1, 0, 0, 0),
                                                 float4( 0, 0, 1, 0),
                                                 float4( 0, 0, 0, 1)), currentNode.transform);
        }
    };

    for (pugi::xml_node library_visual_scene : branch)
    {
        for (pugi::xml_node visualNode : library_visual_scene)
        {
            if (strcmp(visualNode.name(), "node") == 0)
            {
                DAE::Node node;
                parser(visualNode, node);
                data.nodes.push_back(node);
            }
        }
    }
}

void DAE::parse(pugi::xml_node root, DAE::Data &data, Flags flags)
{
    if (flags & Graphics)
    {
        parse_library_images(root.child("library_images"), data);
        parse_library_effects(root.child("library_effects"), data);
        parse_library_materials(root.child("library_materials"), data);
        parse_library_geometries(root.child("library_geometries"), data);
    }
    if (flags & Physics)
    {
        parse_library_physics_models(root.child("library_physics_models"), data);
        parse_library_physics_scenes(root.child("library_physics_scenes"), data);
    }
    if (flags != None)
    {
        parse_library_visual_scenes(root.child("library_visual_scenes"), data);
    }
}

bool DAE::parse(const std::string &filename, DAE::Data & data, Flags flags)
{
    if (std::filesystem::path(filename).extension() != ".dae")
        return false;

    if (!std::filesystem::exists(filename))
        return false;

    data.filename = filename;
    printf("Loading '%s'\n", std::filesystem::absolute(filename).c_str());

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filename.c_str());
    if (result.status != pugi::status_ok)
        return false;

    auto root = doc.child("COLLADA");
    std::string version = root.attribute("version").as_string();
    printf("COLLADA version '%s'\n", version.c_str());
    data.upAxis = root.child("asset").child_value("up_axis");

    parse(root, data, flags);

    fflush(stdout);

    return !data.nodes.empty();
}
