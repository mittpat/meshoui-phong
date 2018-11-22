#pragma once

#include <collada.h>
#include <hashid.h>
#include <linalg.h>

#include <string>
#include <vector>

namespace Meshoui
{
    typedef DAE::Effect::Value MeshMaterialValue;

    class MeshMaterial final
    {
    public:
        static const MeshMaterial kDefault;

        ~MeshMaterial();
        MeshMaterial();
        MeshMaterial(HashId n, const std::vector<MeshMaterialValue> & v);

        HashId name;
        std::vector<MeshMaterialValue> values;
        bool repeatTexcoords;
    };
    inline MeshMaterial::~MeshMaterial() {}
    inline MeshMaterial::MeshMaterial() : repeatTexcoords(false) {}
    inline MeshMaterial::MeshMaterial(HashId n, const std::vector<MeshMaterialValue> & v) : name(n), values(v), repeatTexcoords(false) {}

    struct Attribute final
    {
        HashId name;
        uint32_t offset;
    };

    struct Vertex final
    {
        ~Vertex();
        Vertex();

        linalg::aliases::float3 position;
        linalg::aliases::float2 texcoord;
        linalg::aliases::float3 normal;
        linalg::aliases::float3 tangent;
        linalg::aliases::float3 bitangent;

        static const std::array<Attribute, 5> Attributes;
        static const Attribute & describe(HashId attribute);
    };
    inline Vertex::~Vertex() {}
    inline Vertex::Vertex() : position(linalg::zero), texcoord(linalg::zero), normal(linalg::aliases::float3(0.f, 1.f, 0.f)), tangent(linalg::aliases::float3(1.f, 0.f, 0.f)), bitangent(linalg::aliases::float3(0.f, 0.f, 1.f)) {}

    struct MeshDefinition final
    {
        MeshDefinition();
        MeshDefinition(HashId name, size_t v);
        HashId definitionId;
        bool doubleSided;
        std::vector<unsigned int> indices;
        std::vector<Vertex> vertices;
    };
    inline MeshDefinition::MeshDefinition() : doubleSided(false) {}
    inline MeshDefinition::MeshDefinition(HashId name, size_t v) : definitionId(name), doubleSided(false), vertices(v) {}

    struct MeshInstance final
    {
        MeshInstance();
        MeshInstance(HashId name, const MeshDefinition & definition);
        HashId instanceId;
        HashId definitionId;
        HashId materialId;
        linalg::aliases::float3 scale;
        linalg::aliases::float4 orientation;
        linalg::aliases::float3 position;
    };
    inline MeshInstance::MeshInstance() : scale(1.f, 1.f, 1.f), orientation(linalg::identity), position(0.f, 0.f, 0.f) {}
    inline MeshInstance::MeshInstance(HashId name, const MeshDefinition & definition) : instanceId(name), definitionId(definition.definitionId), scale(1.f, 1.f, 1.f), orientation(linalg::identity), position(0.f, 0.f, 0.f) {}

    struct MeshFile final
    {
        static MeshFile kDefault(const std::string & name, size_t v);

        std::string filename;
        std::vector<MeshDefinition> definitions;
        std::vector<MeshInstance> instances;
        std::vector<MeshMaterial> materials;
    };
    typedef std::vector<MeshFile> MeshFiles;

    namespace MeshLoader
    {
        bool load(const std::string & filename, MeshFile &meshFile);
    }
}
