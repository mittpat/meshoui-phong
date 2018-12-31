#include "vertexformat.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <numeric>
#include <vector>

static constexpr MoFloat3 operator * (const MoFloat3 & a, float b) { return {a.x*b,a.y*b,a.z*b}; }
static constexpr MoFloat2 operator - (const MoFloat2 & a, const MoFloat2 & b) { return {a.x-b.x,a.y-b.y}; }
static constexpr MoFloat3 operator - (const MoFloat3 & a, const MoFloat3 & b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
static constexpr MoFloat3 operator + (const MoFloat3 & a, const MoFloat3 & b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
static           float            dot(const MoFloat3 & a, const MoFloat3 & b) { return std::inner_product(&a.x, &a.x+3, &b.x, 0.0f); }
static           MoFloat3         min(const MoFloat3 & a, const MoFloat3 & b) { return {std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)}; }
static           MoFloat3         max(const MoFloat3 & a, const MoFloat3 & b) { return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)}; }
static constexpr MoFloat3       cross(const MoFloat3 & a, const MoFloat3 & b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
static           MoFloat3   normalize(const MoFloat3 & a)                     { return a * (1.0f / std::sqrt(dot(a, a))); }

#define MO_OCTREE_DOT_PRODUCT_THRESHOLD 0.9f
typedef struct MoOctreeNode {
    MoVertex vertex;
    unsigned int index;
} MoOctreeNode;

static bool equal(const MoFloat2 & a, const MoFloat2 & b, float tol = 1.0e-6) { return std::abs(a.x-b.x) < tol
                                                                                    && std::abs(a.y-b.y) < tol; }
static bool equal(const MoFloat3 & a, const MoFloat3 & b, float tol = 1.0e-6) { return std::abs(a.x-b.x) < tol
                                                                                    && std::abs(a.y-b.y) < tol
                                                                                    && std::abs(a.z-b.z) < tol; }
static bool operator ==(const MoOctreeNode & lhs, const MoVertex &rhs) { return equal(lhs.vertex.position, rhs.position)
                                                                             && equal(lhs.vertex.texcoord, rhs.texcoord)
                                                                             && dot(lhs.vertex.normal, rhs.normal) > MO_OCTREE_DOT_PRODUCT_THRESHOLD; }

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
            if (data.empty() || equal(point.vertex.position, data.front().vertex.position))
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

void moCreateVertexFormat(MoVertexFormatCreateInfo *pCreateInfo, MoVertexFormat *pFormat)
{
    assert((pCreateInfo->indexTypeSize == 1 || pCreateInfo->indexTypeSize == 2 || pCreateInfo->indexTypeSize == 4) && "MoVertexFormatCreateInfo.indexTypeSize must describe index size of 1, 2 or 4 bytes");
    uint32_t triangleCount = pCreateInfo->indexCount;

    MoVertexFormat format = *pFormat = new MoVertexFormat_T();
    format->indexCount = 0;
    format->pIndices = new uint32_t[triangleCount*3]; // actual output size
    format->vertexCount = 0;
    format->pVertices = new MoVertex[triangleCount*3]; // may resize down with indexing

    // indexing begin
    std::unique_ptr<MoOctree> octree = nullptr;
    if ((pCreateInfo->flags & MO_VERTEX_FORMAT_DISABLE_REINDEXING_BIT) == 0)
    {
        struct AABB final
        {
            AABB() : lower{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()},
                     upper{std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()} {}
            void extend(MoFloat3 p) { lower = min(lower, p); upper = max(upper, p); }
            MoFloat3 center() const { return (lower + upper) * 0.5f; }
            MoFloat3 half() const { return (upper - lower) * 0.5f; }
            MoFloat3 lower, upper;
        } bbox;
        const MoVertexAttribute & positionAttribute = pCreateInfo->pAttributes[0];
        assert(positionAttribute.componentCount == 3 && "First MoVertexAttribute of MoVertexFormatCreateInfo must describe a position with 3 components (x,y,z)");
        for (uint32_t i = 0; i < positionAttribute.attributeCount; ++i)
            bbox.extend({pCreateInfo->pAttributes[0].pAttribute[i*3+0],
                         pCreateInfo->pAttributes[0].pAttribute[i*3+1],
                         pCreateInfo->pAttributes[0].pAttribute[i*3+2]});
        octree.reset(new MoOctree(bbox.center(), bbox.half()));
    }
    // indexing end

    uint32_t globalVertexIndexingOffset = 0;
    for (uint32_t i = 0; i < triangleCount; ++i)
    {
        // uint3 vertices, texcoords, normals;
        uint32_t stride = pCreateInfo->attributeCount * 3;
        // one triangle has 3 vertices
        std::vector<uint32_t> vertexIndices[3];
        for (uint32_t j = 0; j < pCreateInfo->attributeCount; ++j)
        {
            struct Index { static uint32_t value(const uint8_t *data, uint32_t typeSize) { switch (typeSize) { case 4: return *(uint32_t*)data; case 1: return *(uint8_t*)data; case 2: return *(uint16_t*)data; }}};
            vertexIndices[0].push_back(Index::value(&pCreateInfo->pIndexes[(0+i*stride+j*3)*pCreateInfo->indexTypeSize], pCreateInfo->indexTypeSize) - ((pCreateInfo->flags & MO_VERTEX_FORMAT_INDICES_COUNT_FROM_ONE_BIT) ? 1 : 0));
            vertexIndices[1].push_back(Index::value(&pCreateInfo->pIndexes[(1+i*stride+j*3)*pCreateInfo->indexTypeSize], pCreateInfo->indexTypeSize) - ((pCreateInfo->flags & MO_VERTEX_FORMAT_INDICES_COUNT_FROM_ONE_BIT) ? 1 : 0));
            vertexIndices[2].push_back(Index::value(&pCreateInfo->pIndexes[(2+i*stride+j*3)*pCreateInfo->indexTypeSize], pCreateInfo->indexTypeSize) - ((pCreateInfo->flags & MO_VERTEX_FORMAT_INDICES_COUNT_FROM_ONE_BIT) ? 1 : 0));
        }

        // one triangle has 3 vertices
        MoVertex *vertex = (MoVertex*)&format->pVertices[3*i-globalVertexIndexingOffset];
        uint32_t localVertexIndexingOffset = 0;
        for (uint32_t k = 0; k < 3; ++k)
        {
            MoVertex &current = vertex[k-localVertexIndexingOffset] = {};
            uint32_t attributeIterator = 0;
            for (uint32_t l = 0; l < vertexIndices[k].size(); ++l)
            {
                const MoVertexAttribute & attribute = pCreateInfo->pAttributes[l];
                std::memcpy(&current.data[attributeIterator], &attribute.pAttribute[vertexIndices[k][l]*attribute.componentCount], sizeof(float)*attribute.componentCount);
                attributeIterator += attribute.componentCount;
            }
            // not generating proper tangents yet
            current.tangent = {1,0,0};
            current.bitangent = {0,0,1};

            bool reuse = false;
            // indexing begin
            if (octree)
            {
                std::vector<MoOctreeNode> nodes;
                octree->getPointsInsideBox(current.position-MoFloat3{0.01,0.01,0.01}, current.position+MoFloat3{0.01,0.01,0.01}, nodes);
                auto found = std::find(nodes.begin(), nodes.end(), current);
                if (found != nodes.end())
                {
                    (uint32_t&)format->pIndices[format->indexCount] = (*found).index;
                    ++globalVertexIndexingOffset;
                    ++localVertexIndexingOffset;
                    reuse = true;
                }
                else
                {
                    octree->insert(MoOctreeNode{current, format->vertexCount});
                }
            }
            // indexing end
            if (!reuse)
            {
                (uint32_t&)format->pIndices[format->indexCount] = format->vertexCount;
                ++format->vertexCount;
            }

            // renormalize and generate tangents
            if (k == 2 && (pCreateInfo->flags & (MO_VERTEX_FORMAT_DISCARD_NORMALS_BIT | MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT)))
            {
                MoVertex &v1 = (MoVertex &)format->pVertices[format->pIndices[format->indexCount-2]];
                MoVertex &v2 = (MoVertex &)format->pVertices[format->pIndices[format->indexCount-1]];
                MoVertex &v3 = (MoVertex &)format->pVertices[format->pIndices[format->indexCount-0]];

                const MoFloat3 edge1 = v2.position - v1.position;
                const MoFloat3 edge2 = v3.position - v1.position;

                if (pCreateInfo->flags & MO_VERTEX_FORMAT_DISCARD_NORMALS_BIT)
                {
                    v1.normal = v2.normal = v3.normal = normalize(cross(edge1, edge2));
                }

                if (pCreateInfo->flags & MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT)
                {
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
                }
            }

            ++format->indexCount;
        }
    }

    format->pVertices = (MoVertex*)realloc((MoVertex*)format->pVertices, format->vertexCount * sizeof(MoVertex));
}

void moDestroyVertexFormat(MoVertexFormat format)
{
    delete[] format->pIndices;
    delete[] format->pVertices;
    delete format;
}
