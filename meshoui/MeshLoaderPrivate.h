#pragma once

#include "hashid.h"

#include <linalg.h>
#include <vector>

struct Attributes final
{
    std::vector<linalg::aliases::float3> vertices;
    std::vector<linalg::aliases::float2> texcoords;
    std::vector<linalg::aliases::float3> normals;
};

struct Triangle final
{
    Triangle();
    linalg::aliases::uint3 vertices;
    linalg::aliases::uint3 texcoords;
    linalg::aliases::uint3 normals;
};
inline Triangle::Triangle() : vertices(linalg::zero), texcoords(linalg::zero), normals(linalg::zero) {}

struct AABB final
{
    AABB();
    void extend(linalg::aliases::float3 p);
    linalg::aliases::float3 center() const;
    linalg::aliases::float3 half() const;
    linalg::aliases::float3 lower;
    linalg::aliases::float3 upper;
};
inline AABB::AABB() : lower(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
                      upper(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()) {}
inline void AABB::extend(linalg::aliases::float3 p) { lower = linalg::min(lower, p); upper = linalg::max(upper, p); }
inline linalg::aliases::float3 AABB::center() const { return (lower + upper) * 0.5f; }
inline linalg::aliases::float3 AABB::half() const { return (upper - lower) * 0.5f; }

struct Geometry final
{
    Geometry();
    std::vector<Triangle> triangles;
    HashId id;
    AABB bbox;
    bool doubleSided;
};
inline Geometry::Geometry() : doubleSided(false) {}

class MeshDefinition;
namespace MeshLoader
{
    void buildGeometry(MeshDefinition & definition, const Attributes & attributes, const Geometry & geometry, bool renormalize = false);
}
