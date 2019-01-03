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
static constexpr bool operator     ==(const MoFloat2 & a, const MoFloat2 & b) { return a.x == b.x && a.y == b.y; }
static constexpr bool operator     ==(const MoFloat3 & a, const MoFloat3 & b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
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

static bool operator ==(const MoOctreeNode & lhs, const MoVertex &rhs) { return lhs.vertex.position == rhs.position && lhs.vertex.texcoord == rhs.texcoord && dot(lhs.vertex.normal, rhs.normal) > MO_OCTREE_DOT_PRODUCT_THRESHOLD; }

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

void moCreateVertexFormat(MoVertexFormatCreateInfo *pCreateInfo, MoVertexFormat *pFormat)
{
    if (pCreateInfo->indexCount > 0)
    {
        assert((pCreateInfo->indexTypeSize == 1 || pCreateInfo->indexTypeSize == 2 || pCreateInfo->indexTypeSize == 4) && "MoVertexFormatCreateInfo.indexTypeSize must describe index size of 1, 2 or 4 bytes");
    }
    assert(pCreateInfo->attributeCount != 0 && "MoVertexFormatCreateInfo.attributeCount must count at least one MoVertexFormatCreateInfo.pAttributes");
    assert(!((pCreateInfo->flags & MO_VERTEX_FORMAT_INDICES_PER_ATTRIBUTE) && (pCreateInfo->flags & MO_VERTEX_FORMAT_DISABLE_REINDEXING_BIT)) && "MoVertexFormatCreateInfo.flags MO_VERTEX_FORMAT_INDICES_PER_ATTRIBUTE is not compatible with MO_VERTEX_FORMAT_DISABLE_REINDEXING_BIT");
    uint32_t triangleCount = pCreateInfo->indexCount > 0 ? pCreateInfo->indexCount / 3 : pCreateInfo->pAttributes[0].attributeCount / 3;
    if (pCreateInfo->flags & MO_VERTEX_FORMAT_INDICES_PER_ATTRIBUTE)
        triangleCount /= pCreateInfo->attributeCount;

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

    uint32_t triangleIndexingStride = 3;
    uint32_t attributeIndexingStride = 0;
    if (pCreateInfo->flags & MO_VERTEX_FORMAT_INDICES_PER_ATTRIBUTE)
    {
        attributeIndexingStride = pCreateInfo->attributeCount;
        triangleIndexingStride *= attributeIndexingStride;
    }
    uint32_t globalVertexIndexingOffset = 0;
    for (uint32_t i = 0; i < triangleCount; ++i)
    {
        struct Index
        {
            static uint32_t value(const uint8_t *data, uint32_t index, uint32_t typeSize, MoVertexFormatCreateFlags flags)
            {
                uint32_t i = 0;
                switch (typeSize)
                {
                case 1: i = *(uint8_t*) (&data[index*typeSize]); break;
                case 2: i = *(uint16_t*)(&data[index*typeSize]); break;
                case 4: i = *(uint32_t*)(&data[index*typeSize]); break;
                }
                return i - ((flags & MO_VERTEX_FORMAT_INDICES_COUNT_FROM_ONE_BIT) ? 1 : 0);
            }
        };
        // one triangle has 3 vertices
        std::vector<uint32_t> vertexIndices[3];
        // vertexIndices[0] will contain all attribute indexes for first vertex of triangle, vertexIndices[1] will contain all attribute indexes for second vertex, etc
        // loop supports indexes of type uint8, uint16 and uint32 via indexTypeSize
        // loop supports one-based indexes (collada) via flags
        // loop supports per attribute indexing (collada) via flags
        // loop supports only providing vertices without indexes
        for (uint32_t j = 0; j < pCreateInfo->attributeCount; ++j)
        {
            if (pCreateInfo->indexCount > 0)
            {
                vertexIndices[0].push_back(Index::value(pCreateInfo->pIndices, 0+i*triangleIndexingStride+j*attributeIndexingStride, pCreateInfo->indexTypeSize, pCreateInfo->flags));
                vertexIndices[1].push_back(Index::value(pCreateInfo->pIndices, 1+i*triangleIndexingStride+j*attributeIndexingStride, pCreateInfo->indexTypeSize, pCreateInfo->flags));
                vertexIndices[2].push_back(Index::value(pCreateInfo->pIndices, 2+i*triangleIndexingStride+j*attributeIndexingStride, pCreateInfo->indexTypeSize, pCreateInfo->flags));
            }
            else
            {
                vertexIndices[0].push_back(0+i*triangleIndexingStride);
                vertexIndices[1].push_back(1+i*triangleIndexingStride);
                vertexIndices[2].push_back(2+i*triangleIndexingStride);
            }
        }

        // per vertex for triangle
        for (uint32_t k = 0; k < 3; ++k)
        {
            assert(format->indexCount == 3*i+k);

            // pointer to next vertex in output vertex array. if matching vertex is found in octree it will be reused and further addressing will be offset back by one
            MoVertex *current = nullptr;
            if (octree)
                current = (MoVertex*)&format->pVertices[3*i+k-globalVertexIndexingOffset];
            else
                current = (MoVertex*)&format->pVertices[Index::value(pCreateInfo->pIndices, 3*i+k, pCreateInfo->indexTypeSize, pCreateInfo->flags)];
            *current = {};
            current->tangent = {1,0,0};
            current->bitangent = {0,0,1};

            // copy attribute values to new vertex
            uint32_t attributeIterator = 0;
            for (uint32_t l = 0; l < vertexIndices[k].size(); ++l)
            {
                const MoVertexAttribute & attribute = pCreateInfo->pAttributes[l];
                std::memcpy(&current->data[attributeIterator], &attribute.pAttribute[vertexIndices[k][l] * attribute.componentCount], sizeof(float)*attribute.componentCount);
                attributeIterator += attribute.componentCount;
            }

            // indexing begin
            if (octree)
            {
                // look for reusable candidates in octree
                std::vector<MoOctreeNode> nodes;
                octree->getPointsInsideBox(current->position, current->position, nodes);
                // find an exact match in candidates
                auto found = std::find(nodes.begin(), nodes.end(), *current);
                if (found != nodes.end())
                {
                    // reuse existing vertex and offset back by one
                    (uint32_t&)format->pIndices[format->indexCount] = (*found).index;
                    ++globalVertexIndexingOffset;
                }
                else
                {
                    // add new vertex to octree for future potential reuse
                    octree->insert(MoOctreeNode{*current, format->vertexCount});
                    // add an index pointing to last vertex added
                    (uint32_t&)format->pIndices[format->indexCount] = format->vertexCount;
                    ++format->vertexCount;
                }
            }
            // indexing end
            else
            {
                (uint32_t&)format->pIndices[format->indexCount] = Index::value(pCreateInfo->pIndices, 3*i+k, pCreateInfo->indexTypeSize, pCreateInfo->flags);
                format->vertexCount = pCreateInfo->pAttributes[0].attributeCount;
            }

            // renormalize and generate tangents
            if (k == 2 && (pCreateInfo->flags & (MO_VERTEX_FORMAT_DISCARD_NORMALS_BIT | MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT)))
            {
                // once per triangle (k==2) take three last indexed vertices and compute normals and tangents
                MoVertex &v1 = (MoVertex &)format->pVertices[format->pIndices[format->indexCount-2]];
                MoVertex &v2 = (MoVertex &)format->pVertices[format->pIndices[format->indexCount-1]];
                MoVertex &v3 = (MoVertex &)format->pVertices[format->pIndices[format->indexCount-0]];

                const MoFloat3 edge1 = v2.position - v1.position;
                const MoFloat3 edge2 = v3.position - v1.position;

                // renormalize
                if (pCreateInfo->flags & MO_VERTEX_FORMAT_DISCARD_NORMALS_BIT)
                {
                    v1.normal = v2.normal = v3.normal = normalize(cross(edge1, edge2));
                }

                // generate tangents
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

            // increment index counter for each vertex
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

template <typename T, size_t N> size_t countof(T (& arr)[N]) { return std::extent<T[N]>::value; }

void moTestVertexFormat_collada()
{
    static MoFloat3 test_positions[] = { { -1.0f, -1.0f, -1.0f },
                                         { -1.0f, -1.0f,  1.0f },
                                         { -1.0f,  1.0f, -1.0f },
                                         { 1.0f,  1.0f, -1.0f } };
    static MoFloat2 test_texcoords[] = { { 1.0f, 0.0f },
                                         { 0.0f, 1.0f },
                                         { 0.0f, 0.0f } };
    static MoFloat3 test_normals[] = { { -1.0f, 0.0f, 0.0f },
                                       { 0.0f, 0.0f, -1.0f }};
    static MoUInt3x3 test_triangles[] = { { MoUInt3{ 2, 3, 1 }, MoUInt3{ 1, 2, 3 }, MoUInt3{ 1, 1, 1 } },
                                          { MoUInt3{ 4, 1, 3 }, MoUInt3{ 1, 2, 3 }, MoUInt3{ 2, 2, 2 } }};

    MoVertexFormat vertexFormat;

    std::vector<MoVertexAttribute> attributes(3);
    attributes[0].pAttribute = test_positions[0].data;
    attributes[0].attributeCount = (uint32_t)countof(test_positions);
    attributes[0].componentCount = 3;
    attributes[1].pAttribute = test_texcoords[0].data;
    attributes[1].attributeCount = (uint32_t)countof(test_texcoords);
    attributes[1].componentCount = 2;
    attributes[2].pAttribute = test_normals[0].data;
    attributes[2].attributeCount = (uint32_t)countof(test_normals);
    attributes[2].componentCount = 3;

    MoVertexFormatCreateInfo createInfo = {};
    createInfo.pAttributes = attributes.data();
    createInfo.attributeCount = (uint32_t)attributes.size();
    createInfo.pIndices = (uint8_t*)test_triangles->data;
    createInfo.indexCount = (uint32_t)countof(test_triangles)*3*createInfo.attributeCount;
    createInfo.indexTypeSize = sizeof(uint32_t);
    createInfo.flags = MO_VERTEX_FORMAT_INDICES_COUNT_FROM_ONE_BIT | MO_VERTEX_FORMAT_INDICES_PER_ATTRIBUTE | MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT;
    moCreateVertexFormat(&createInfo, &vertexFormat);

    assert(vertexFormat->indexCount == 6);
    assert(vertexFormat->vertexCount == 6);

    static MoVertex test_expected_vertices[] = {
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{1.0f,1.0f,-1.0f},  MoFloat2{1.0f,0.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,1.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,0.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}}};

    for (uint32_t i = 0; i < 6; ++i)
    {
        const MoVertex & v = vertexFormat->pVertices[vertexFormat->pIndices[i]];
        const MoVertex & r = test_expected_vertices[i];
        assert(v.position.x  == r.position.x);
        assert(v.position.y  == r.position.y);
        assert(v.position.z  == r.position.z);
        assert(v.texcoord.x  == r.texcoord.x);
        assert(v.texcoord.y  == r.texcoord.y);
        assert(v.normal.x    == r.normal.x);
        assert(v.normal.y    == r.normal.y);
        assert(v.normal.z    == r.normal.z);
        assert(v.tangent.x   == r.tangent.x);
        assert(v.tangent.y   == r.tangent.y);
        assert(v.tangent.z   == r.tangent.z);
        assert(v.bitangent.x == r.bitangent.x);
        assert(v.bitangent.y == r.bitangent.y);
        assert(v.bitangent.z == r.bitangent.z);
    }

    moDestroyVertexFormat(vertexFormat);
}

void moTestVertexFormat_singleByteIndexes()
{
    static MoFloat3 test_positions[] = { {-1.0f,-1.0f,1.0f},
                                         {-1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f},
                                         {1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f},
                                         {-1.0f,1.0f,-1.0f} };
    static MoFloat2 test_texcoords[] = { {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f},
                                         {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f} };
    static MoFloat3 test_normals[] = { {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {0.0f,0.0f,-1.0f},
                                       {0.0f,0.0f,-1.0f},
                                       {0.0f,0.0f,-1.0f} };
    static uint8_t test_triangles[] = {0,1,2,3,4,5};

    MoVertexFormat vertexFormat;

    std::vector<MoVertexAttribute> attributes(3);
    attributes[0].pAttribute = test_positions[0].data;
    attributes[0].attributeCount = (uint32_t)countof(test_positions);
    attributes[0].componentCount = 3;
    attributes[1].pAttribute = test_texcoords[0].data;
    attributes[1].attributeCount = (uint32_t)countof(test_texcoords);
    attributes[1].componentCount = 2;
    attributes[2].pAttribute = test_normals[0].data;
    attributes[2].attributeCount = (uint32_t)countof(test_normals);
    attributes[2].componentCount = 3;

    MoVertexFormatCreateInfo createInfo = {};
    createInfo.pAttributes = attributes.data();
    createInfo.attributeCount = (uint32_t)attributes.size();
    createInfo.pIndices = test_triangles;
    createInfo.indexCount = (uint32_t)countof(test_triangles);
    createInfo.indexTypeSize = sizeof(uint8_t);
    createInfo.flags = MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT;
    moCreateVertexFormat(&createInfo, &vertexFormat);

    assert(vertexFormat->indexCount == 6);
    assert(vertexFormat->vertexCount == 6);

    static MoVertex test_expected_vertices[] = {
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{1.0f,1.0f,-1.0f},  MoFloat2{1.0f,0.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,1.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,0.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}}};

    for (uint32_t i = 0; i < 6; ++i)
    {
        const MoVertex & v = vertexFormat->pVertices[vertexFormat->pIndices[i]];
        const MoVertex & r = test_expected_vertices[i];
        assert(v.position.x  == r.position.x);
        assert(v.position.y  == r.position.y);
        assert(v.position.z  == r.position.z);
        assert(v.texcoord.x  == r.texcoord.x);
        assert(v.texcoord.y  == r.texcoord.y);
        assert(v.normal.x    == r.normal.x);
        assert(v.normal.y    == r.normal.y);
        assert(v.normal.z    == r.normal.z);
        assert(v.tangent.x   == r.tangent.x);
        assert(v.tangent.y   == r.tangent.y);
        assert(v.tangent.z   == r.tangent.z);
        assert(v.bitangent.x == r.bitangent.x);
        assert(v.bitangent.y == r.bitangent.y);
        assert(v.bitangent.z == r.bitangent.z);
    }

    moDestroyVertexFormat(vertexFormat);
}

void moTestVertexFormat_renormalize()
{
    static MoFloat3 test_positions[] = { {-1.0f,-1.0f,1.0f},
                                         {-1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f},
                                         {1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f},
                                         {-1.0f,1.0f,-1.0f} };
    static MoFloat2 test_texcoords[] = { {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f},
                                         {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f} };
    static MoFloat3 test_normals[] = { {0.0f,1.0f,0.0f},
                                       {0.0f,1.0f,0.0f},
                                       {0.0f,1.0f,0.0f},
                                       {0.0f,1.0f,0.0f},
                                       {0.0f,1.0f,0.0f},
                                       {0.0f,1.0f,0.0f} };
    static uint8_t test_triangles[] = {0,1,2,3,4,5};

    MoVertexFormat vertexFormat;

    std::vector<MoVertexAttribute> attributes(3);
    attributes[0].pAttribute = test_positions[0].data;
    attributes[0].attributeCount = (uint32_t)countof(test_positions);
    attributes[0].componentCount = 3;
    attributes[1].pAttribute = test_texcoords[0].data;
    attributes[1].attributeCount = (uint32_t)countof(test_texcoords);
    attributes[1].componentCount = 2;
    attributes[2].pAttribute = test_normals[0].data;
    attributes[2].attributeCount = (uint32_t)countof(test_normals);
    attributes[2].componentCount = 3;

    MoVertexFormatCreateInfo createInfo = {};
    createInfo.pAttributes = attributes.data();
    createInfo.attributeCount = (uint32_t)attributes.size();
    createInfo.pIndices = test_triangles;
    createInfo.indexCount = (uint32_t)countof(test_triangles);
    createInfo.indexTypeSize = sizeof(uint8_t);
    createInfo.flags = MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT | MO_VERTEX_FORMAT_DISCARD_NORMALS_BIT;
    moCreateVertexFormat(&createInfo, &vertexFormat);

    assert(vertexFormat->indexCount == 6);
    assert(vertexFormat->vertexCount == 6);

    static MoVertex test_expected_vertices[] = {
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{1.0f,1.0f,-1.0f},  MoFloat2{1.0f,0.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,1.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,0.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}}};

    for (uint32_t i = 0; i < 6; ++i)
    {
        const MoVertex & v = vertexFormat->pVertices[vertexFormat->pIndices[i]];
        const MoVertex & r = test_expected_vertices[i];
        assert(v.position.x  == r.position.x);
        assert(v.position.y  == r.position.y);
        assert(v.position.z  == r.position.z);
        assert(v.texcoord.x  == r.texcoord.x);
        assert(v.texcoord.y  == r.texcoord.y);
        assert(v.normal.x    == r.normal.x);
        assert(v.normal.y    == r.normal.y);
        assert(v.normal.z    == r.normal.z);
        assert(v.tangent.x   == r.tangent.x);
        assert(v.tangent.y   == r.tangent.y);
        assert(v.tangent.z   == r.tangent.z);
        assert(v.bitangent.x == r.bitangent.x);
        assert(v.bitangent.y == r.bitangent.y);
        assert(v.bitangent.z == r.bitangent.z);
    }

    moDestroyVertexFormat(vertexFormat);
}

void moTestVertexFormat_unindexed()
{
    static MoFloat3 test_positions[] = { {-1.0f,-1.0f,1.0f},
                                         {-1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f},
                                         {1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f},
                                         {-1.0f,1.0f,-1.0f} };
    static MoFloat2 test_texcoords[] = { {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f},
                                         {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f} };
    static MoFloat3 test_normals[] = { {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {0.0f,0.0f,-1.0f},
                                       {0.0f,0.0f,-1.0f},
                                       {0.0f,0.0f,-1.0f} };

    MoVertexFormat vertexFormat;

    std::vector<MoVertexAttribute> attributes(3);
    attributes[0].pAttribute = test_positions[0].data;
    attributes[0].attributeCount = (uint32_t)countof(test_positions);
    attributes[0].componentCount = 3;
    attributes[1].pAttribute = test_texcoords[0].data;
    attributes[1].attributeCount = (uint32_t)countof(test_texcoords);
    attributes[1].componentCount = 2;
    attributes[2].pAttribute = test_normals[0].data;
    attributes[2].attributeCount = (uint32_t)countof(test_normals);
    attributes[2].componentCount = 3;

    MoVertexFormatCreateInfo createInfo = {};
    createInfo.pAttributes = attributes.data();
    createInfo.attributeCount = (uint32_t)attributes.size();
    createInfo.flags = MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT;
    moCreateVertexFormat(&createInfo, &vertexFormat);

    assert(vertexFormat->indexCount == 6);
    assert(vertexFormat->vertexCount == 6);

    static MoVertex test_expected_vertices[] = {
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{1.0f,1.0f,-1.0f},  MoFloat2{1.0f,0.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,1.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,0.0f},MoFloat3{0.0f,0.0f,-1.0f},MoFloat3{1.0f,0.0f,0.0f},MoFloat3{0.0f,-1.0f,0.0f}}};

    for (uint32_t i = 0; i < 6; ++i)
    {
        const MoVertex & v = vertexFormat->pVertices[vertexFormat->pIndices[i]];
        const MoVertex & r = test_expected_vertices[i];
        assert(v.position.x  == r.position.x);
        assert(v.position.y  == r.position.y);
        assert(v.position.z  == r.position.z);
        assert(v.texcoord.x  == r.texcoord.x);
        assert(v.texcoord.y  == r.texcoord.y);
        assert(v.normal.x    == r.normal.x);
        assert(v.normal.y    == r.normal.y);
        assert(v.normal.z    == r.normal.z);
        assert(v.tangent.x   == r.tangent.x);
        assert(v.tangent.y   == r.tangent.y);
        assert(v.tangent.z   == r.tangent.z);
        assert(v.bitangent.x == r.bitangent.x);
        assert(v.bitangent.y == r.bitangent.y);
        assert(v.bitangent.z == r.bitangent.z);
    }

    moDestroyVertexFormat(vertexFormat);
}

void moTestVertexFormat_reindex()
{
    static MoFloat3 test_positions[] = { {-1.0f,-1.0f,1.0f},
                                         {-1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f},
                                         {-1.0f,-1.0f,1.0f},
                                         {-1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f} };
    static MoFloat2 test_texcoords[] = { {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f},
                                         {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f} };
    static MoFloat3 test_normals[] = { {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f} };
    static uint8_t test_triangles[] = {0,1,2,3,4,5};

    MoVertexFormat vertexFormat;

    std::vector<MoVertexAttribute> attributes(3);
    attributes[0].pAttribute = test_positions[0].data;
    attributes[0].attributeCount = (uint32_t)countof(test_positions);
    attributes[0].componentCount = 3;
    attributes[1].pAttribute = test_texcoords[0].data;
    attributes[1].attributeCount = (uint32_t)countof(test_texcoords);
    attributes[1].componentCount = 2;
    attributes[2].pAttribute = test_normals[0].data;
    attributes[2].attributeCount = (uint32_t)countof(test_normals);
    attributes[2].componentCount = 3;

    MoVertexFormatCreateInfo createInfo = {};
    createInfo.pAttributes = attributes.data();
    createInfo.attributeCount = (uint32_t)attributes.size();
    createInfo.pIndices = test_triangles;
    createInfo.indexCount = (uint32_t)countof(test_triangles);
    createInfo.indexTypeSize = sizeof(uint8_t);
    createInfo.flags = MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT;
    moCreateVertexFormat(&createInfo, &vertexFormat);

    assert(vertexFormat->indexCount == 6);
    assert(vertexFormat->vertexCount == 3);

    static MoVertex test_expected_vertices[] = {
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}}};

    for (uint32_t i = 0; i < 6; ++i)
    {
        const MoVertex & v = vertexFormat->pVertices[vertexFormat->pIndices[i]];
        const MoVertex & r = test_expected_vertices[i];
        assert(v.position.x  == r.position.x);
        assert(v.position.y  == r.position.y);
        assert(v.position.z  == r.position.z);
        assert(v.texcoord.x  == r.texcoord.x);
        assert(v.texcoord.y  == r.texcoord.y);
        assert(v.normal.x    == r.normal.x);
        assert(v.normal.y    == r.normal.y);
        assert(v.normal.z    == r.normal.z);
        assert(v.tangent.x   == r.tangent.x);
        assert(v.tangent.y   == r.tangent.y);
        assert(v.tangent.z   == r.tangent.z);
        assert(v.bitangent.x == r.bitangent.x);
        assert(v.bitangent.y == r.bitangent.y);
        assert(v.bitangent.z == r.bitangent.z);
    }

    moDestroyVertexFormat(vertexFormat);
}

void moTestVertexFormat_noReindex()
{
    static MoFloat3 test_positions[] = { {-1.0f,-1.0f,1.0f},
                                         {-1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f} };
    static MoFloat2 test_texcoords[] = { {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f} };
    static MoFloat3 test_normals[] = { {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f} };
    static uint8_t test_triangles[] = {0,1,2,0,1,2};

    MoVertexFormat vertexFormat;

    std::vector<MoVertexAttribute> attributes(3);
    attributes[0].pAttribute = test_positions[0].data;
    attributes[0].attributeCount = (uint32_t)countof(test_positions);
    attributes[0].componentCount = 3;
    attributes[1].pAttribute = test_texcoords[0].data;
    attributes[1].attributeCount = (uint32_t)countof(test_texcoords);
    attributes[1].componentCount = 2;
    attributes[2].pAttribute = test_normals[0].data;
    attributes[2].attributeCount = (uint32_t)countof(test_normals);
    attributes[2].componentCount = 3;

    MoVertexFormatCreateInfo createInfo = {};
    createInfo.pAttributes = attributes.data();
    createInfo.attributeCount = (uint32_t)attributes.size();
    createInfo.pIndices = test_triangles;
    createInfo.indexCount = (uint32_t)countof(test_triangles);
    createInfo.indexTypeSize = sizeof(uint8_t);
    createInfo.flags = MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT | MO_VERTEX_FORMAT_DISABLE_REINDEXING_BIT;
    moCreateVertexFormat(&createInfo, &vertexFormat);

    assert(vertexFormat->indexCount == 6);
    assert(vertexFormat->vertexCount == 3);

    for (uint32_t i = 0; i < 6; ++i) { assert(vertexFormat->pIndices[i] == test_triangles[i]); }

    static MoVertex test_expected_vertices[] = {
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}}};

    for (uint32_t i = 0; i < 6; ++i)
    {
        const MoVertex & v = vertexFormat->pVertices[vertexFormat->pIndices[i]];
        const MoVertex & r = test_expected_vertices[i];
        assert(v.position.x  == r.position.x);
        assert(v.position.y  == r.position.y);
        assert(v.position.z  == r.position.z);
        assert(v.texcoord.x  == r.texcoord.x);
        assert(v.texcoord.y  == r.texcoord.y);
        assert(v.normal.x    == r.normal.x);
        assert(v.normal.y    == r.normal.y);
        assert(v.normal.z    == r.normal.z);
        assert(v.tangent.x   == r.tangent.x);
        assert(v.tangent.y   == r.tangent.y);
        assert(v.tangent.z   == r.tangent.z);
        assert(v.bitangent.x == r.bitangent.x);
        assert(v.bitangent.y == r.bitangent.y);
        assert(v.bitangent.z == r.bitangent.z);
    }

    moDestroyVertexFormat(vertexFormat);
}

void moTestVertexFormat_noReindex_uint16()
{
    static MoFloat3 test_positions[] = { {-1.0f,-1.0f,1.0f},
                                         {-1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f} };
    static MoFloat2 test_texcoords[] = { {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f} };
    static MoFloat3 test_normals[] = { {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f} };
    static uint16_t test_triangles[] = {0,1,2,0,1,2};

    MoVertexFormat vertexFormat;

    std::vector<MoVertexAttribute> attributes(3);
    attributes[0].pAttribute = test_positions[0].data;
    attributes[0].attributeCount = (uint32_t)countof(test_positions);
    attributes[0].componentCount = 3;
    attributes[1].pAttribute = test_texcoords[0].data;
    attributes[1].attributeCount = (uint32_t)countof(test_texcoords);
    attributes[1].componentCount = 2;
    attributes[2].pAttribute = test_normals[0].data;
    attributes[2].attributeCount = (uint32_t)countof(test_normals);
    attributes[2].componentCount = 3;

    MoVertexFormatCreateInfo createInfo = {};
    createInfo.pAttributes = attributes.data();
    createInfo.attributeCount = (uint32_t)attributes.size();
    createInfo.pIndices = (uint8_t*)test_triangles;
    createInfo.indexCount = (uint32_t)countof(test_triangles);
    createInfo.indexTypeSize = sizeof(uint16_t);
    createInfo.flags = MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT | MO_VERTEX_FORMAT_DISABLE_REINDEXING_BIT;
    moCreateVertexFormat(&createInfo, &vertexFormat);

    assert(vertexFormat->indexCount == 6);
    assert(vertexFormat->vertexCount == 3);

    for (uint32_t i = 0; i < 6; ++i) { assert(vertexFormat->pIndices[i] == test_triangles[i]); }

    static MoVertex test_expected_vertices[] = {
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}}};

    for (uint32_t i = 0; i < 6; ++i)
    {
        const MoVertex & v = vertexFormat->pVertices[vertexFormat->pIndices[i]];
        const MoVertex & r = test_expected_vertices[i];
        assert(v.position.x  == r.position.x);
        assert(v.position.y  == r.position.y);
        assert(v.position.z  == r.position.z);
        assert(v.texcoord.x  == r.texcoord.x);
        assert(v.texcoord.y  == r.texcoord.y);
        assert(v.normal.x    == r.normal.x);
        assert(v.normal.y    == r.normal.y);
        assert(v.normal.z    == r.normal.z);
        assert(v.tangent.x   == r.tangent.x);
        assert(v.tangent.y   == r.tangent.y);
        assert(v.tangent.z   == r.tangent.z);
        assert(v.bitangent.x == r.bitangent.x);
        assert(v.bitangent.y == r.bitangent.y);
        assert(v.bitangent.z == r.bitangent.z);
    }

    moDestroyVertexFormat(vertexFormat);
}

void moTestVertexFormat_noReindexCountFromOne()
{
    static MoFloat3 test_positions[] = { {-1.0f,-1.0f,1.0f},
                                         {-1.0f,1.0f,-1.0f},
                                         {-1.0f,-1.0f,-1.0f} };
    static MoFloat2 test_texcoords[] = { {1.0f,0.0f},
                                         {0.0f,1.0f},
                                         {0.0f,0.0f} };
    static MoFloat3 test_normals[] = { {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f},
                                       {-1.0f,0.0f,0.0f} };
    static uint8_t test_triangles[] = {1,2,3,1,2,3};

    MoVertexFormat vertexFormat;

    std::vector<MoVertexAttribute> attributes(3);
    attributes[0].pAttribute = test_positions[0].data;
    attributes[0].attributeCount = (uint32_t)countof(test_positions);
    attributes[0].componentCount = 3;
    attributes[1].pAttribute = test_texcoords[0].data;
    attributes[1].attributeCount = (uint32_t)countof(test_texcoords);
    attributes[1].componentCount = 2;
    attributes[2].pAttribute = test_normals[0].data;
    attributes[2].attributeCount = (uint32_t)countof(test_normals);
    attributes[2].componentCount = 3;

    MoVertexFormatCreateInfo createInfo = {};
    createInfo.pAttributes = attributes.data();
    createInfo.attributeCount = (uint32_t)attributes.size();
    createInfo.pIndices = test_triangles;
    createInfo.indexCount = (uint32_t)countof(test_triangles);
    createInfo.indexTypeSize = sizeof(uint8_t);
    createInfo.flags = MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT | MO_VERTEX_FORMAT_DISABLE_REINDEXING_BIT | MO_VERTEX_FORMAT_INDICES_COUNT_FROM_ONE_BIT;
    moCreateVertexFormat(&createInfo, &vertexFormat);

    assert(vertexFormat->indexCount == 6);
    assert(vertexFormat->vertexCount == 3);

    for (uint32_t i = 0; i < 6; ++i) { assert(vertexFormat->pIndices[i] == test_triangles[i]-1); }

    static MoVertex test_expected_vertices[] = {
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,1.0f}, MoFloat2{1.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,1.0f,-1.0f}, MoFloat2{0.0f,1.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}},
        {MoFloat3{-1.0f,-1.0f,-1.0f},MoFloat2{0.0f,0.0f},MoFloat3{-1.0f,0.0f,0.0f},MoFloat3{0.0f,0.0f,1.0f}, MoFloat3{0.0f,1.0f,0.0f}}};

    for (uint32_t i = 0; i < 6; ++i)
    {
        const MoVertex & v = vertexFormat->pVertices[vertexFormat->pIndices[i]];
        const MoVertex & r = test_expected_vertices[i];
        assert(v.position.x  == r.position.x);
        assert(v.position.y  == r.position.y);
        assert(v.position.z  == r.position.z);
        assert(v.texcoord.x  == r.texcoord.x);
        assert(v.texcoord.y  == r.texcoord.y);
        assert(v.normal.x    == r.normal.x);
        assert(v.normal.y    == r.normal.y);
        assert(v.normal.z    == r.normal.z);
        assert(v.tangent.x   == r.tangent.x);
        assert(v.tangent.y   == r.tangent.y);
        assert(v.tangent.z   == r.tangent.z);
        assert(v.bitangent.x == r.bitangent.x);
        assert(v.bitangent.y == r.bitangent.y);
        assert(v.bitangent.z == r.bitangent.z);
    }

    moDestroyVertexFormat(vertexFormat);
}

void moTestVertexFormat()
{
    moTestVertexFormat_collada();
    moTestVertexFormat_singleByteIndexes();
    moTestVertexFormat_renormalize();
    moTestVertexFormat_unindexed();
    moTestVertexFormat_reindex();
    moTestVertexFormat_noReindex();
    moTestVertexFormat_noReindex_uint16();
    moTestVertexFormat_noReindexCountFromOne();
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
