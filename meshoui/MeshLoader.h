#pragma once

#include "hashid.h"
#include "linalg.h"

#include <string>
#include <vector>

class MeshMaterialValue final
{
public:
    ~MeshMaterialValue();
    MeshMaterialValue();
    MeshMaterialValue(HashId n, const std::vector<float> & d);
    MeshMaterialValue(HashId n, const std::string & f);

    HashId name;
    std::vector<float> data;
    std::string filename;
};
inline MeshMaterialValue::~MeshMaterialValue() {}
inline MeshMaterialValue::MeshMaterialValue() : name() {}
inline MeshMaterialValue::MeshMaterialValue(HashId n, const std::vector<float> & d) : name(n), data(d) {}
inline MeshMaterialValue::MeshMaterialValue(HashId n, const std::string & f) : name(n), filename(f) {}

class MeshMaterial final
{
public:
    ~MeshMaterial();
    MeshMaterial();

    HashId name;
    std::vector<MeshMaterialValue> values;
};
inline MeshMaterial::~MeshMaterial() {}
inline MeshMaterial::MeshMaterial() {}

struct Vertex final
{
    ~Vertex();
    Vertex();

    linalg::aliases::float3 position;
    linalg::aliases::float2 texcoord;
    linalg::aliases::float3 normal;
    linalg::aliases::float3 tangent;
    linalg::aliases::float3 bitangent;

    static const std::array<HashId, 5> Attributes;
    static const size_t AttributeDataSize;
};
inline Vertex::~Vertex() {}
inline Vertex::Vertex() : position(linalg::zero), texcoord(linalg::zero), normal(linalg::zero), tangent(linalg::zero), bitangent(linalg::zero) {}

struct MeshDefinition final
{
    MeshDefinition();
    HashId definitionId;
    bool doubleSided;
    std::vector<unsigned int> indices;
    std::vector<Vertex> vertices;
};
inline MeshDefinition::MeshDefinition() : doubleSided(false) {}

struct MeshInstance final
{
    MeshInstance();
    HashId definitionId;
    HashId instanceId;
    HashId materialId;
    linalg::aliases::float3 scale;
    linalg::aliases::float4 orientation;
    linalg::aliases::float3 position;
};
inline MeshInstance::MeshInstance() : scale(1.f, 1.f, 1.f), orientation(linalg::rotation_quat(linalg::aliases::float3(0.f, 1.f, 0.f), 0.f)), position(0.f, 0.f, 0.f) {}

struct MeshFile final
{
    HashId filename;
    std::vector<MeshDefinition> definitions;
    std::vector<MeshInstance> instances;
    std::vector<MeshMaterial> materials;
};
typedef std::vector<MeshFile> MeshCache;

namespace MeshLoader
{
    bool load   (const std::string & filename, MeshFile &fileCache);
    bool loadDae(const std::string & filename, MeshFile &fileCache);
}
