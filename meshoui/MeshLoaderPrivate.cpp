#include "MeshLoaderPrivate.h"
#include "MeshLoader.h"

#include <algorithm>
#include <collada.h>
#include <linalg.h>
#include <loose.h>

using namespace linalg;
using namespace linalg::aliases;

#include <octree.h>

using namespace Meshoui;

namespace
{
    #define DOT_PRODUCT_THRESHOLD 0.9
    struct Node
    {
        Node(const Vertex & v, unsigned int i);
        float3 position() const;
        bool operator==(const Vertex & rhs) const;
        Vertex vertex;
        unsigned int index;
    };
    inline Node::Node(const Vertex & v, unsigned int i) : vertex(v), index(i) {}
    inline float3 Node::position() const { return vertex.position; }
    inline bool Node::operator==(const Vertex &rhs) const { return vertex.position == rhs.position && vertex.texcoord == rhs.texcoord && linalg::dot(vertex.normal, rhs.normal) > DOT_PRODUCT_THRESHOLD; }
}

void MeshLoader::buildGeometry(MeshDefinition & definition, const DAE::Mesh & mesh, bool renormalize)
{
    linalg::AABB bbox;
    for (const auto & vertex : mesh.vertices)
        bbox.extend({vertex.x,vertex.y,vertex.z});

    std::vector<Node> nodes;
    nodes.reserve(sqrt(mesh.triangles.size()));
    Octree<Node> octree(bbox.center(), bbox.half());

    printf("Loading '%s'\n", definition.definitionId.str.empty() ? "(unnamed root)" : definition.definitionId.str.c_str());

    // indexed
    for (size_t i = 0; i < mesh.triangles.size(); ++i)
    {
        std::array<Vertex, 3> avertex;

        const auto & ivertices = mesh.triangles[i].vertices;
        avertex[0].position = (const float3&)mesh.vertices[ivertices.x-1];
        avertex[1].position = (const float3&)mesh.vertices[ivertices.y-1];
        avertex[2].position = (const float3&)mesh.vertices[ivertices.z-1];

        const auto & itexcoords = mesh.triangles[i].texcoords;
        if (itexcoords.x-1 < mesh.texcoords.size())
            avertex[0].texcoord = (const float2&)mesh.texcoords[itexcoords.x-1];
        if (itexcoords.y-1 < mesh.texcoords.size())
            avertex[1].texcoord = (const float2&)mesh.texcoords[itexcoords.y-1];
        if (itexcoords.z-1 < mesh.texcoords.size())
            avertex[2].texcoord = (const float2&)mesh.texcoords[itexcoords.z-1];

        if (renormalize)
        {
            float3 a = avertex[1].position - avertex[0].position;
            float3 b = avertex[2].position - avertex[0].position;
            float3 normal = normalize(cross(a, b));
            avertex[0].normal = normal;
            avertex[1].normal = normal;
            avertex[2].normal = normal;
        }
        else
        {
            const auto & inormals = mesh.triangles[i].normals;
            if (inormals.x-1 < mesh.normals.size())
                avertex[0].normal = (const float3&)mesh.normals[inormals.x-1];
            if (inormals.y-1 < mesh.normals.size())
                avertex[1].normal = (const float3&)mesh.normals[inormals.y-1];
            if (inormals.z-1 < mesh.normals.size())
                avertex[2].normal = (const float3&)mesh.normals[inormals.z-1];
        }

        // tangent + bitangent
        float3 edge1 = avertex[1].position - avertex[0].position;
        float3 edge2 = avertex[2].position - avertex[0].position;
        float2 deltaUV1 = avertex[1].texcoord - avertex[0].texcoord;
        float2 deltaUV2 = avertex[2].texcoord - avertex[0].texcoord;

        float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (f != 0.f)
        {
            f = 1.0f / f;

            float3 tangent(0.f,0.f,0.f);
            tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            avertex[0].tangent = avertex[1].tangent = avertex[2].tangent = normalize(tangent);

            float3 bitangent(0.f,0.f,0.f);
            bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
            bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
            bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
            avertex[0].bitangent = avertex[1].bitangent = avertex[2].bitangent = normalize(bitangent);
        }

        for (auto & vertex : avertex)
        {
            nodes.clear();
            octree.getPointsInsideBox(vertex.position, vertex.position, nodes);
            auto found = std::find(nodes.begin(), nodes.end(), vertex);
            if (found == nodes.end())
            {
                definition.vertices.push_back(vertex);
                definition.indices.push_back(definition.vertices.size() - 1);
                octree.insert(Node(vertex, definition.indices.back()));
            }
            else
            {
                definition.indices.push_back((*found).index);
            }
        }
    }
}
