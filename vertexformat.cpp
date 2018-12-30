#include "vertexformat.h"

#include <array>
#include <cstring>
#include <numeric>
#include <vector>

#define MO_OCTREE_DOT_PRODUCT_THRESHOLD 0.9f
typedef struct MoOctreeNode {
    MoVertex vertex;
    unsigned int index;
} MoOctreeNode;

static constexpr MoFloat3 operator * (const MoFloat3 & a, float b) { return {a.x*b,a.y*b,a.z*b}; }
static constexpr MoFloat3 operator - (const MoFloat3 & a, const MoFloat3 & b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
static constexpr MoFloat3 operator + (const MoFloat3 & a, const MoFloat3 & b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
static constexpr bool     operator ==(const MoFloat2 & a, const MoFloat2 & b) { return a.x==b.x&&a.y==b.y; }
static constexpr bool     operator ==(const MoFloat3 & a, const MoFloat3 & b) { return a.x==b.x&&a.y==b.y&&a.z==b.z; }
static           float            dot(const MoFloat3 & a, const MoFloat3 & b) { return std::inner_product(&a.x, &a.x+3, &b.x, 0.0f); }

static constexpr bool     operator ==(const MoOctreeNode & lhs, const MoVertex &rhs) { return lhs.vertex.position == rhs.position && lhs.vertex.texcoord == rhs.texcoord && dot(lhs.vertex.normal, rhs.normal) > MO_OCTREE_DOT_PRODUCT_THRESHOLD; }

struct MoOctree final
{
    ~MoOctree() { for (auto * child : children) { delete child; } }
    MoOctree(const MoFloat3 & origin, const MoFloat3 & halfDimension) : origin(origin), halfDimension(halfDimension), children({}), data() {}
    MoOctree(const MoOctree & copy) : origin(copy.origin), halfDimension(copy.halfDimension), data(copy.data) {}
    bool isLeafNode() const { return children[0] == nullptr; }
    unsigned getOctantContainingPoint(const MoFloat3 & point) const {
        unsigned oct = 0;
        if (point.x >= origin.x) oct |= 4;
        if (point.y >= origin.y) oct |= 2;
        if (point.z >= origin.z) oct |= 1;
        return oct;
    }
    void insert(const MoOctreeNode & point) {
        if (isLeafNode())
        {
            if (data.empty() || point.vertex.position == data.front().vertex.position)
            {
                data.push_back(point);
                return;
            }
            else
            {
                MoFloat3 nextHalfDimension = halfDimension*.5f;
                if (nextHalfDimension.x == 0.f || nextHalfDimension.y == 0.f || nextHalfDimension.z == 0.f)
                {
                    data.push_back(point);
                }
                else
                {
                    for (unsigned i = 0; i < 8; ++i)
                    {
                        MoFloat3 newOrigin = origin;
                        newOrigin.x += halfDimension.x * (i&4 ? .5f : -.5f);
                        newOrigin.y += halfDimension.y * (i&2 ? .5f : -.5f);
                        newOrigin.z += halfDimension.z * (i&1 ? .5f : -.5f);
                        children[i] = new MoOctree(newOrigin, nextHalfDimension);
                    }
                    for (const auto & oldPoint : data)
                    {
                        children[getOctantContainingPoint(oldPoint.vertex.position)]->insert(oldPoint);
                    }
                    data.clear();
                    children[getOctantContainingPoint(point.vertex.position)]->insert(point);
                }
            }
        }
        else
        {
            unsigned octant = getOctantContainingPoint(point.vertex.position);
            children[octant]->insert(point);
        }
    }
    void getPointsInsideBox(const MoFloat3 & bmin, const MoFloat3 & bmax, std::vector<MoOctreeNode> & results) {
        if (isLeafNode())
        {
            if (!data.empty())
            {
                for (const auto & point : data)
                {
                    const MoFloat3 p = point.vertex.position;
                    if (p.x > bmax.x || p.y > bmax.y || p.z > bmax.z) return;
                    if (p.x < bmin.x || p.y < bmin.y || p.z < bmin.z) return;
                    results.push_back(point);
                }
            }
        }
        else
        {
            for (auto * child : children)
            {
                const MoFloat3 cmax = child->origin + child->halfDimension;
                const MoFloat3 cmin = child->origin - child->halfDimension;
                if (cmax.x < bmin.x || cmax.y < bmin.y || cmax.z < bmin.z) continue;
                if (cmin.x > bmax.x || cmin.y > bmax.y || cmin.z > bmax.z) continue;
                child->getPointsInsideBox(bmin, bmax, results);
            }
        }
    }

    MoFloat3 origin, halfDimension;
    std::array<MoOctree*, 8> children;
    std::vector<MoOctreeNode> data;
};
#if 0
void buildGeometry(Meshoui::MeshDefinition & definition, const DAE::Mesh & mesh, bool renormalize = false)
{
    struct AABB final
    {
        AABB() : lower(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
                 upper(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()) {}
        void extend(MoFloat3 p) { lower = min(lower, p); upper = max(upper, p); }
        MoFloat3 center() const { return (lower + upper) * 0.5f; }
        MoFloat3 half() const { return (upper - lower) * 0.5f; }
        MoFloat3 lower, upper;
    } bbox;
    for (const auto & vertex : mesh.vertices)
        bbox.extend({vertex.x,vertex.y,vertex.z});

    std::vector<Node> nodes;
    nodes.reserve(unsigned(sqrt(mesh.triangles.size())));
    Octree<Node> octree(bbox.center(), bbox.half());

    printf("Loading '%s'\n", definition.definitionId.str.empty() ? "(unnamed root)" : definition.definitionId.str.c_str());

    // indexed
    for (size_t i = 0; i < mesh.triangles.size(); ++i)
    {
        std::array<MoVertex, 3> avertex;

        const auto & ivertices = mesh.triangles[i].vertices;
        avertex[0].position = mesh.vertices[ivertices.x-1];
        avertex[1].position = mesh.vertices[ivertices.y-1];
        avertex[2].position = mesh.vertices[ivertices.z-1];

        const auto & itexcoords = mesh.triangles[i].texcoords;
        if (itexcoords.x-1 < mesh.texcoords.size())
            avertex[0].texcoord = mesh.texcoords[itexcoords.x-1];
        if (itexcoords.y-1 < mesh.texcoords.size())
            avertex[1].texcoord = mesh.texcoords[itexcoords.y-1];
        if (itexcoords.z-1 < mesh.texcoords.size())
            avertex[2].texcoord = mesh.texcoords[itexcoords.z-1];

        if (renormalize)
        {
            MoFloat3 a = avertex[1].position - avertex[0].position;
            MoFloat3 b = avertex[2].position - avertex[0].position;
            MoFloat3 normal = normalize(cross(a, b));
            avertex[0].normal = normal;
            avertex[1].normal = normal;
            avertex[2].normal = normal;
        }
        else
        {
            const auto & inormals = mesh.triangles[i].normals;
            if (inormals.x-1 < mesh.normals.size())
                avertex[0].normal = mesh.normals[inormals.x-1];
            if (inormals.y-1 < mesh.normals.size())
                avertex[1].normal = mesh.normals[inormals.y-1];
            if (inormals.z-1 < mesh.normals.size())
                avertex[2].normal = mesh.normals[inormals.z-1];
        }

        // tangent + bitangent
        MoFloat3 edge1 = avertex[1].position - avertex[0].position;
        MoFloat3 edge2 = avertex[2].position - avertex[0].position;
        float2 deltaUV1 = avertex[1].texcoord - avertex[0].texcoord;
        float2 deltaUV2 = avertex[2].texcoord - avertex[0].texcoord;

        float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (f != 0.f)
        {
            f = 1.0f / f;

            MoFloat3 tangent(0.f,0.f,0.f);
            tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            avertex[0].tangent = avertex[1].tangent = avertex[2].tangent = normalize(tangent);

            MoFloat3 bitangent(0.f,0.f,0.f);
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
                definition.indices.push_back(unsigned(definition.vertices.size()) - 1);
                octree.insert(Node(vertex, definition.indices.back()));
            }
            else
            {
                definition.indices.push_back((*found).index);
            }
        }
    }
}
#endif
void moCreateVertexFormat(MoVertexFormatCreateInfo *pCreateInfo, MoVertexFormat *pFormat)
{
    MoVertexFormat format = *pFormat = new MoVertexFormat_T();
    format->indexCount = pCreateInfo->indexCount*3;
    format->pIndices = new uint32_t[format->indexCount];
    format->vertexCount = format->indexCount;
    format->pVertices = new MoVertex[format->indexCount];

    //std::vector<uint32_t> outIndices;
    //std::vector<MoVertex> outVertices;
    for (uint32_t i = 0; i < pCreateInfo->indexCount; ++i)
    {
        // uint3 vertices, texcoords, normals;
        uint32_t stride = pCreateInfo->attributeCount * 3;
        // one triangle has 3 vertices
        std::vector<uint32_t> vertexIndices[3];//, vertex2Indices, vertex3Indices;
        for (uint32_t j = 0; j < pCreateInfo->attributeCount; ++j)
        {
            vertexIndices[0].push_back(pCreateInfo->pIndexes[0+i*stride+j*3] - (pCreateInfo->indicesCountFromOne ? 1 : 0));
            vertexIndices[1].push_back(pCreateInfo->pIndexes[1+i*stride+j*3] - (pCreateInfo->indicesCountFromOne ? 1 : 0));
            vertexIndices[2].push_back(pCreateInfo->pIndexes[2+i*stride+j*3] - (pCreateInfo->indicesCountFromOne ? 1 : 0));
        }

        // one triangle has 3 vertices
        MoVertex *vertex = (MoVertex *)&format->pVertices[3*i];
        for (uint32_t k = 0; k < 3; ++k)
        {
            vertex[k] = {};
            uint32_t attributeIterator = 0;
            for (uint32_t l = 0; l < vertexIndices[k].size(); ++l)
            {
                const MoVertexAttribute & attribute = pCreateInfo->pAttributes[l];
                std::memcpy(&vertex[k].data[attributeIterator], &attribute.pAttribute[vertexIndices[k][l]*attribute.componentCount], sizeof(float)*attribute.componentCount);
                attributeIterator += attribute.componentCount;
            }
        }
    }
    // not using indexes for now
    for (uint32_t m = 0; m < format->indexCount; ++m)
    {
        (uint32_t&)format->pIndices[m] = m;
    }

    return;
#if 0
    std::vector<uint32_t> indices;
    std::vector<MoVertex> vertices;
#define INDICES_COUNT_FROM_ONE
#ifdef INDICES_COUNT_FROM_ONE
    for (const auto & triangle : cube_triangles)
    {
        vertices.emplace_back(MoVertex{ cube_positions[triangle.x.x - 1], cube_texcoords[triangle.y.x - 1], cube_normals[triangle.z.x - 1], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size());
        vertices.emplace_back(MoVertex{ cube_positions[triangle.x.y - 1], cube_texcoords[triangle.y.y - 1], cube_normals[triangle.z.y - 1], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size());
        vertices.emplace_back(MoVertex{ cube_positions[triangle.x.z - 1], cube_texcoords[triangle.y.z - 1], cube_normals[triangle.z.z - 1], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size());
    }
    for (uint32_t & index : indices) { --index; }
#else
    for (const auto & triangle : cube_triangles)
    {
        vertices.emplace_back(MoVertex{ cube_positions[triangle.x.x], cube_texcoords[triangle.y.x], cube_normals[triangle.z.x], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size()-1);
        vertices.emplace_back(MoVertex{ cube_positions[triangle.x.y], cube_texcoords[triangle.y.y], cube_normals[triangle.z.y], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size()-1);
        vertices.emplace_back(MoVertex{ cube_positions[triangle.x.z], cube_texcoords[triangle.y.z], cube_normals[triangle.z.z], {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}); indices.push_back((uint32_t)vertices.size()-1);
    }
#endif
    for (uint32_t index = 0; index < indices.size(); index+=3)
    {
        MoVertex &v1 = vertices[indices[index+0]];
        MoVertex &v2 = vertices[indices[index+1]];
        MoVertex &v3 = vertices[indices[index+2]];

        //discardNormals
        const MoFloat3 edge1 = v2.position - v1.position;
        const MoFloat3 edge2 = v3.position - v1.position;
        v1.normal = v2.normal = v3.normal = normalize(cross(edge1, edge2));

#define GENERATE_TANGENTS
#ifdef GENERATE_TANGENTS
        const MoFloat2 deltaUV1 = v2.texcoord - v1.texcoord;
        const MoFloat2 deltaUV2 = v3.texcoord - v1.texcoord;
        float f = deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y;
        if (f != 0.f)
        {
            f = 1.0f / f;

            v1.tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
            v1.tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
            v1.tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
            v1.tangent = v2.tangent = v3.tangent = normalize(v1.tangent);
            v1.bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
            v1.bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
            v1.bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
            v1.bitangent = v2.bitangent = v3.bitangent = normalize(v1.bitangent);
        }
#endif
    }
#endif
}

void moDestroyVertexFormat(MoVertexFormat format)
{
    delete[] format->pIndices;
    delete format;
}
