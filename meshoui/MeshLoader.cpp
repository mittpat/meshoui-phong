#include "MeshLoader.h"

#include <linalg.h>
using namespace linalg;
using namespace linalg::aliases;
#define MESHOUI_COLLADA_LINALG
#include <collada.h>
#include <functional>

#include <experimental/filesystem>

namespace std { namespace filesystem = experimental::filesystem; }
namespace
{
    std::string sibling(const std::string & path, const std::string & other)
    {
        std::filesystem::path parentpath(other);
        std::filesystem::path parentdirectory = parentpath.parent_path();
        return (parentdirectory / path).u8string();
    }

    #define DOT_PRODUCT_THRESHOLD 0.9f
    struct Node
    {
        Node(const Meshoui::Vertex & v, unsigned int i) : vertex(v), index(i) {}
        float3 position() const { return vertex.position; }
        bool operator==(const Meshoui::Vertex &rhs) const { return vertex.position == rhs.position && vertex.texcoord == rhs.texcoord && linalg::dot(vertex.normal, rhs.normal) > DOT_PRODUCT_THRESHOLD; }
        Meshoui::Vertex vertex;
        unsigned int index;
    };

    template<typename T>
    struct Octree final
    {
        ~Octree() { for (auto * child : children) { delete child; } }
        Octree(const float3 & origin, const float3 & halfDimension) : origin(origin), halfDimension(halfDimension), children({}), data() {}
        Octree(const Octree & copy) : origin(copy.origin), halfDimension(copy.halfDimension), data(copy.data) {}
        bool isLeafNode() const { return children[0] == nullptr; }

        unsigned getOctantContainingPoint(const float3 & point) const
        {
            unsigned oct = 0;
            if (point.x >= origin.x) oct |= 4;
            if (point.y >= origin.y) oct |= 2;
            if (point.z >= origin.z) oct |= 1;
            return oct;
        }

        void insert(const T & point)
        {
            if (isLeafNode())
            {
                if (data.empty() || point.position() == data.front().position())
                {
                    data.push_back(point);
                    return;
                }
                else
                {
                    float3 nextHalfDimension = halfDimension*.5f;
                    if (nextHalfDimension.x == 0.f || nextHalfDimension.y == 0.f || nextHalfDimension.z == 0.f)
                    {
                        data.push_back(point);
                    }
                    else
                    {
                        for (unsigned i = 0; i < 8; ++i)
                        {
                            float3 newOrigin = origin;
                            newOrigin.x += halfDimension.x * (i&4 ? .5f : -.5f);
                            newOrigin.y += halfDimension.y * (i&2 ? .5f : -.5f);
                            newOrigin.z += halfDimension.z * (i&1 ? .5f : -.5f);
                            children[i] = new Octree(newOrigin, nextHalfDimension);
                        }
                        for (const auto & oldPoint : data)
                        {
                            children[getOctantContainingPoint(oldPoint.position())]->insert(oldPoint);
                        }
                        data.clear();
                        children[getOctantContainingPoint(point.position())]->insert(point);
                    }
                }
            }
            else
            {
                unsigned octant = getOctantContainingPoint(point.position());
                children[octant]->insert(point);
            }
        }

        void getPointsInsideBox(const float3 & bmin, const float3 & bmax, std::vector<T> & results)
        {
            if (isLeafNode())
            {
                if (!data.empty())
                {
                    for (const auto & point : data)
                    {
                        const float3 p = point.position();
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
                    const float3 cmax = child->origin + child->halfDimension;
                    const float3 cmin = child->origin - child->halfDimension;
                    if (cmax.x < bmin.x || cmax.y < bmin.y || cmax.z < bmin.z) continue;
                    if (cmin.x > bmax.x || cmin.y > bmax.y || cmin.z > bmax.z) continue;
                    child->getPointsInsideBox(bmin, bmax, results);
                }
            }
        }

        float3 origin, halfDimension;
        std::array<Octree*, 8> children;
        std::vector<T> data;
    };
}

using namespace Meshoui;

MeshDefinition MeshLoader::makeGeometry(const DAE::Geometry &geometry, const DAE::Data &data, bool renormalize)
{
    MeshDefinition ret;
    ret.definitionId = HashId(geometry.id, data.filename);

    struct AABB final
    {
        AABB() : lower(std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
                 upper(std::numeric_limits<float>::min(), std::numeric_limits<float>::min(), std::numeric_limits<float>::min()) {}
        void extend(float3 p) { lower = min(lower, p); upper = max(upper, p); }
        float3 center() const { return (lower + upper) * 0.5f; }
        float3 half() const { return (upper - lower) * 0.5f; }
        float3 lower, upper;
    } bbox;
    for (const auto & vertex : geometry.mesh.vertices)
        bbox.extend({vertex.x,vertex.y,vertex.z});

    std::vector<Node> nodes;
    nodes.reserve(unsigned(sqrt(geometry.mesh.triangles.size())));
    Octree<Node> octree(bbox.center(), bbox.half());

    // indexed
    for (size_t i = 0; i < geometry.mesh.triangles.size(); ++i)
    {
        std::array<Meshoui::Vertex, 3> avertex;

        const auto & ivertices = geometry.mesh.triangles[i].vertices;
        avertex[0].position = geometry.mesh.vertices[ivertices.x-1];
        avertex[1].position = geometry.mesh.vertices[ivertices.y-1];
        avertex[2].position = geometry.mesh.vertices[ivertices.z-1];

        const auto & itexcoords = geometry.mesh.triangles[i].texcoords;
        if (itexcoords.x-1 < geometry.mesh.texcoords.size())
            avertex[0].texcoord = geometry.mesh.texcoords[itexcoords.x-1];
        if (itexcoords.y-1 < geometry.mesh.texcoords.size())
            avertex[1].texcoord = geometry.mesh.texcoords[itexcoords.y-1];
        if (itexcoords.z-1 < geometry.mesh.texcoords.size())
            avertex[2].texcoord = geometry.mesh.texcoords[itexcoords.z-1];

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
            const auto & inormals = geometry.mesh.triangles[i].normals;
            if (inormals.x-1 < geometry.mesh.normals.size())
                avertex[0].normal = geometry.mesh.normals[inormals.x-1];
            if (inormals.y-1 < geometry.mesh.normals.size())
                avertex[1].normal = geometry.mesh.normals[inormals.y-1];
            if (inormals.z-1 < geometry.mesh.normals.size())
                avertex[2].normal = geometry.mesh.normals[inormals.z-1];
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
                ret.vertices.push_back(vertex);
                ret.indices.push_back(unsigned(ret.vertices.size()) - 1);
                octree.insert(Node(vertex, ret.indices.back()));
            }
            else
            {
                ret.indices.push_back((*found).index);
            }
        }
    }
    return ret;
}

MeshMaterial MeshLoader::makeMaterial(const DAE::Material &material, const DAE::Data &data)
{
    MeshMaterial ret;
    ret.materialId = HashId(material.id, data.filename);
    auto effect = std::find_if(data.effects.begin(), data.effects.end(), [material](const DAE::Effect & effect){ return effect.id == material.effect.url; });
    for (const auto & value : (*effect).values)
    {
        DAE::Effect::Value v = value;
        auto image = std::find_if(data.images.begin(), data.images.end(), [v](const DAE::Image & image){ return image.id == v.texture; });
        if (image != data.images.end()) { v.texture = (*image).initFrom; }

        if (v.sid == "uniformAmbient") { ret.ambient = (float3&)v.data[0]; }
        else if (v.sid == "uniformTextureAmbient") { ret.textureAmbient = sibling(v.texture, data.filename); }
        else if (v.sid == "uniformDiffuse") { ret.diffuse = (float3&)v.data[0]; }
        else if (v.sid == "uniformTextureDiffuse") { ret.textureDiffuse = sibling(v.texture, data.filename); }
        else if (v.sid == "uniformSpecular") { ret.specular = (float3&)v.data[0]; }
        else if (v.sid == "uniformTextureSpecular") { ret.textureSpecular = sibling(v.texture, data.filename); }
        else if (v.sid == "uniformEmissive") { ret.emissive = (float3&)v.data[0]; }
        else if (v.sid == "uniformTextureEmissive") { ret.textureEmissive = sibling(v.texture, data.filename); }
        else if (v.sid == "uniformTextureNormal") { ret.textureNormal = sibling(v.texture, data.filename); }
    }
    return ret;
}

bool MeshLoader::load(const std::string &filename, MeshFile &meshFile)
{
    DAE::Data data;
    if (DAE::parse(filename, data, DAE::Graphics))
    {
        meshFile.filename = data.filename;
        meshFile.materials.push_back(MeshMaterial());
        for (const auto & libraryMaterial : data.materials)
        {
            meshFile.materials.push_back(makeMaterial(libraryMaterial, data));
        }
        for (const auto & libraryGeometry : data.geometries)
        {
            meshFile.definitions.push_back(makeGeometry(libraryGeometry, data));
        }
        for (const auto & libraryNode : data.nodes)
        {
            if (!libraryNode.geometry.name.empty())
            {
                MeshInstance instance;
                instance.instanceId = libraryNode.id;
                instance.definitionId = HashId(libraryNode.geometry.url, filename);
                if (!libraryNode.geometry.material.empty())
                    instance.materialId = HashId(libraryNode.geometry.material, filename);
                else
                    instance.materialId = meshFile.materials[0].materialId;
                instance.modelMatrix = libraryNode.transform;
                meshFile.instances.push_back(instance);
            }
        }
        return !meshFile.definitions.empty() && !meshFile.instances.empty();
    }
    return false;
}

void MeshLoader::cube(const std::string &name, MeshFile &meshFile)
{
    meshFile.filename = name;
    meshFile.materials.resize(1);
    meshFile.materials[0].materialId = name + "_cube_mat";

    DAE::Geometry geometry;
    geometry.id = name + "_cube_def";
    geometry.mesh.vertices = {{-1.0, -1.0, -1.0},
                              {-1.0, -1.0,  1.0},
                              {-1.0,  1.0, -1.0},
                              {-1.0,  1.0,  1.0},
                              { 1.0, -1.0, -1.0},
                              { 1.0, -1.0,  1.0},
                              { 1.0,  1.0, -1.0},
                              { 1.0,  1.0,  1.0}};
    geometry.mesh.texcoords = {{1,0},
                               {0,2},
                               {0,0},
                               {1,2}};
    geometry.mesh.triangles = {{{2,3,1}, {1,2,3}, {0,0,0}},
                               {{4,7,3}, {1,2,3}, {0,0,0}},
                               {{8,5,7}, {1,2,3}, {0,0,0}},
                               {{6,1,5}, {1,2,3}, {0,0,0}},
                               {{7,1,3}, {1,2,3}, {0,0,0}},
                               {{4,6,8}, {1,2,3}, {0,0,0}},
                               {{2,4,3}, {1,4,2}, {0,0,0}},
                               {{4,8,7}, {1,4,2}, {0,0,0}},
                               {{8,6,5}, {1,4,2}, {0,0,0}},
                               {{6,2,1}, {1,4,2}, {0,0,0}},
                               {{7,5,1}, {1,4,2}, {0,0,0}},
                               {{4,2,6}, {1,4,2}, {0,0,0}}};
    meshFile.definitions.resize(1);
    meshFile.definitions[0] = makeGeometry(geometry, DAE::Data(), true);

    meshFile.instances.resize(1);
    meshFile.instances[0].definitionId = meshFile.definitions[0].definitionId;
    meshFile.instances[0].materialId = meshFile.materials[0].materialId;
    meshFile.instances[0].instanceId = name + "_cube_inst";
}
