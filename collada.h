#pragma once

#include <string>
#include <vector>

namespace pugi { class xml_node; }

namespace DAE
{
#if !defined(MESHOUI_COLLADA_LINALG)
    struct uint3 final { unsigned x,y,z; };
    struct float2 final { float x,y; };
    struct float3 final { float x,y,z; };
    struct float4 final { float x,y,z,w; };
    struct float4x4 final { float4 x,y,z,w; };
#if !defined(MESHOUI_COLLADA_CONSTANTS_H)
#define MESHOUI_COLLADA_CONSTANTS_H 1
    const float4x4 identity = {{1.f,0.f,0.f,0.f},
                               {0.f,1.f,0.f,0.f},
                               {0.f,0.f,1.f,0.f},
                               {0.f,0.f,0.f,1.f}};
#endif
#endif

    struct Image final
    {
        std::string id, initFrom;
    };

    struct Surface final
    {
        std::string sid, initFrom;
    };

    struct Sampler final
    {
        std::string sid, source;
    };

    struct Effect final
    {
        struct Value final
        {
            Value(const std::string & i, const std::vector<float> & d);
            Value(const std::string & i, const std::string & t);
            std::string sid;
            std::vector<float> data;
            std::string texture;
        };

        std::string id;
        std::vector<Surface> surfaces;
        std::vector<Sampler> samplers;
        std::vector<Value> values;
        const std::string & solve(const std::string & v) const;
    };
    inline Effect::Value::Value(const std::string & i, const std::vector<float> & d) : sid(i), data(d) {}
    inline Effect::Value::Value(const std::string & i, const std::string & t) : sid(i), texture(t) {}

    struct InstanceEffect final
    {
        std::string url;
    };

    struct Material final
    {
        std::string id;
        InstanceEffect effect;
    };

    struct Triangle final
    {
        Triangle();
        Triangle(const uint3 & v, const uint3 & t, const uint3 & n);
        uint3 vertices, texcoords, normals;
    };
    inline Triangle::Triangle() : vertices{0,0,0}, texcoords{0,0,0}, normals{0,0,0} {}
    inline Triangle::Triangle(const uint3 & v, const uint3 & t, const uint3 & n) : vertices(v), texcoords(t), normals(n) {}

    struct Mesh final
    {
        std::vector<Triangle> triangles;
        std::vector<float3> vertices;
        std::vector<float2> texcoords;
        std::vector<float3> normals;
    };

    struct Geometry final
    {
        Geometry();
        std::string id;
        Mesh mesh;
        bool doubleSided;
    };
    inline Geometry::Geometry() : doubleSided(false) {}

    struct InstanceGeometry final
    {
        std::string url, name, material;
    };

    struct Node final
    {
        Node();
        std::string id;
        float4x4 transform;
        InstanceGeometry geometry;
    };
    inline Node::Node() : transform(identity) {}

    struct Shape final
    {
        Shape();
        float3 halfExtents;
    };
    inline Shape::Shape() : halfExtents{1.f, 1.f, 1.f} {}

    struct RigidBody final
    {
        RigidBody();
        std::string sid;
        Shape shape;
        bool dynamic;
    };
    inline RigidBody::RigidBody() : dynamic(true) {}

    struct PhysicsModel final
    {
        std::string id;
        std::vector<RigidBody> bodies;
    };

    struct InstancePhysicsModel final
    {
        std::string sid, url, parent;
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

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2018 Patrick Pelletier
Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
software, either in source code form or as a compiled binary, for any purpose,
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this
software dedicate any and all copyright interest in the software to the public
domain. We make this dedication for the benefit of the public at large and to
the detriment of our heirs and successors. We intend this dedication to be an
overt act of relinquishment in perpetuity of all present and future rights to
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
