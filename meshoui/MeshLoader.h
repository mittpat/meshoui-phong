#pragma once

#include <linalg.h>
#include "hashid.h"

#include <string>
#include <vector>

namespace DAE
{
    struct Data;
    struct Geometry;
    struct Material;
}

namespace Meshoui
{
    struct MeshMaterial final
    {
        MeshMaterial();
        HashId materialId;
        linalg::aliases::float3 ambient;
        linalg::aliases::float3 diffuse;
        linalg::aliases::float3 specular;
        linalg::aliases::float3 emissive;
        std::string textureAmbient;
        std::string textureDiffuse;
        std::string textureNormal;
        std::string textureSpecular;
        std::string textureEmissive;
    };
    inline MeshMaterial::MeshMaterial() : ambient(0.0f, 0.0f, 0.0f), diffuse(0.64f, 0.64f, 0.64f), specular(0.5f, 0.5f, 0.5f), emissive(0.0f, 0.0f, 0.0f) {}

    struct Vertex final
    {
        Vertex();
        linalg::aliases::float3 position;
        linalg::aliases::float2 texcoord;
        linalg::aliases::float3 normal;
        linalg::aliases::float3 tangent;
        linalg::aliases::float3 bitangent;
    };
    inline Vertex::Vertex() : position(linalg::zero), texcoord(linalg::zero), normal(linalg::aliases::float3(0.f, 1.f, 0.f)), tangent(linalg::aliases::float3(1.f, 0.f, 0.f)), bitangent(linalg::aliases::float3(0.f, 0.f, 1.f)) {}

    struct MeshDefinition final
    {
        HashId definitionId;
        std::vector<unsigned int> indices;
        std::vector<Vertex> vertices;
    };

    struct MeshInstance final
    {
        MeshInstance();
        HashId instanceId;
        HashId definitionId;
        HashId materialId;
        linalg::aliases::float4x4 modelMatrix;
    };
    inline MeshInstance::MeshInstance() : modelMatrix(linalg::identity) {}

    struct MeshFile final
    {
        std::string filename;
        std::vector<MeshDefinition> definitions;
        std::vector<MeshInstance> instances;
        std::vector<MeshMaterial> materials;
    };

    class MeshPrivate;
    class MaterialPrivate;
    struct SimpleMesh final
    {
        SimpleMesh();
        MeshDefinition geometry;
        MeshMaterial material;
        linalg::aliases::float4x4 modelMatrix;

        MeshPrivate * d;
        MaterialPrivate * m;
    };
    inline SimpleMesh::SimpleMesh() : modelMatrix(linalg::identity), d(nullptr), m(nullptr) {}

    namespace MeshLoader
    {
        MeshDefinition makeGeometry(const DAE::Geometry & geometry, const DAE::Data & data, bool renormalize = false);
        MeshMaterial makeMaterial(const DAE::Material & material, const DAE::Data & data);

        bool load(const std::string & filename, MeshFile &meshFile);
        void cube(const std::string & name, MeshFile &meshFile);
    }
}
