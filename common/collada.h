#pragma once

#include <optional>
#include <linalg.h>
#include <vector>
#include "hashid.h"
#include "loose.h"

namespace pugi { class xml_node; }

namespace DAE
{
    struct Image final
    {
        HashId id;
        std::string initFrom;
    };

    struct Surface final
    {
        HashId sid;
        std::string initFrom;
    };

    struct Sampler final
    {
        HashId sid;
        std::string source;
    };

    struct Effect final
    {
        struct Value final
        {
            Value(HashId i, const std::vector<float> & d);
            Value(HashId i, const std::string & t);

            HashId sid;
            std::optional<std::vector<float>> data;
            std::optional<std::string> texture;
        };

        HashId id;
        std::vector<Surface> surfaces;
        std::vector<Sampler> samplers;
        std::vector<Value> values;
        std::string solve(HashId v) const;
    };
    inline Effect::Value::Value(HashId i, const std::vector<float> & d) : sid(i), data(d) {}
    inline Effect::Value::Value(HashId i, const std::string & t) : sid(i), texture(t) {}

    struct InstanceEffect final
    {
        HashId url;
    };

    struct Material final
    {
        HashId id;
        InstanceEffect effect;
    };

    struct Triangle final
    {
        Triangle();
        linalg::aliases::uint3 vertices;
        linalg::aliases::uint3 texcoords;
        linalg::aliases::uint3 normals;
    };
    inline Triangle::Triangle() : vertices(linalg::zero), texcoords(linalg::zero), normals(linalg::zero) {}

    struct Mesh final
    {
        AABB bbox;
        std::vector<Triangle> triangles;
        std::vector<linalg::aliases::float3> vertices;
        std::vector<linalg::aliases::float2> texcoords;
        std::vector<linalg::aliases::float3> normals;
    };

    struct Geometry final
    {
        Geometry();
        HashId id;
        bool doubleSided;
        Mesh mesh;
    };
    inline Geometry::Geometry() : doubleSided(false) {}

    struct InstanceGeometry final
    {
        HashId url;
        HashId name;
        HashId material;
    };

    struct Node final
    {
        Node();
        HashId id;
        linalg::aliases::float4x4 transform;
        std::optional<InstanceGeometry> geometry;
    };
    inline Node::Node() : transform(linalg::identity) {}

    struct Shape final
    {
        Shape();
        linalg::aliases::float3 halfExtents;
    };
    inline Shape::Shape() : halfExtents(1.f, 1.f, 1.f) {}

    struct RigidBody final
    {
        RigidBody();
        HashId sid;
        Shape shape;
        bool dynamic;
    };
    inline RigidBody::RigidBody() : dynamic(true) {}

    struct PhysicsModel final
    {
        HashId id;
        std::vector<RigidBody> bodies;
    };

    struct InstancePhysicsModel final
    {
        HashId sid;
        HashId url;
        HashId parent;
    };

    struct Data final
    {
        std::string filename;
        std::string upAxis;
        std::vector<DAE::Image> images;
        std::vector<DAE::Effect> effects;
        std::vector<DAE::Material> materials;
        std::vector<DAE::Geometry> geometries;
        std::vector<DAE::Node> nodes;
        std::vector<DAE::PhysicsModel> models;
        std::vector<DAE::InstancePhysicsModel> instances;
    };

    enum { None = 0x0, Graphics = 0x01, Physics = 0x02, All = Graphics | Physics };
    typedef int Flags;

    void parse_effect_profile_phong(pugi::xml_node branch, Effect & effect);
    void parse_effect_profile(pugi::xml_node branch, Effect & effect);
    void parse_geometry_mesh(pugi::xml_node branch, Geometry & geometry);
    void parse_library_images(pugi::xml_node branch, Data & data);
    void parse_library_effects(pugi::xml_node branch, Data & data);
    void parse_library_materials(pugi::xml_node branch, Data & data);
    void parse_library_geometries(pugi::xml_node branch, Data & data);
    void parse_library_physics_models(pugi::xml_node branch, Data & data);
    void parse_library_physics_scenes(pugi::xml_node branch, Data & data);
    void parse_library_visual_scenes(pugi::xml_node branch, Data & data);
    void parse(pugi::xml_node root, Data & data, Flags flags = All);
    bool parse(const std::string & filename, Data & data, Flags flags = All);
}
