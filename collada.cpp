#include "collada.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <functional>
#include <pugixml.hpp>

namespace DAE
{
    struct Image
    {
        std::string id, initFrom;
    };

    struct Surface
    {
        std::string sid, initFrom;
    };

    struct Sampler
    {
        std::string sid, source;
    };

    struct Effect
    {
        struct Value
        {
            Value(const std::string & i, const std::vector<float> & d) : sid(i), data(d) {}
            Value(const std::string & i, const std::string & t) : sid(i), texture(t) {}
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

    struct LibraryData
    {
        std::string upAxis;
        std::vector<DAE::Image> images;
        std::vector<DAE::Effect> effects;
    };
}

static uint32_t nextPowerOfTwo(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

static constexpr MoFloat4 operator * (const MoFloat4 & a, float b) { return {a.x*b,a.y*b,a.z*b,a.w*b}; }
static constexpr MoFloat4 operator + (const MoFloat4 & a, const MoFloat4 & b) { return {a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w}; }
static constexpr MoFloat4 mul(const MoFloat4x4 & a, const MoFloat4 & b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
static constexpr MoFloat4x4 mul(const MoFloat4x4 & a, const MoFloat4x4 & b) { return {mul(a,b.x), mul(a,b.y), mul(a,b.z), mul(a,b.w)}; }
static constexpr MoFloat3 qxdir(const MoFloat4 & q) { return {q.w*q.w+q.x*q.x-q.y*q.y-q.z*q.z, (q.x*q.y+q.z*q.w)*2, (q.z*q.x-q.y*q.w)*2}; }
static constexpr MoFloat3 qydir(const MoFloat4 & q) { return {(q.x*q.y-q.z*q.w)*2, q.w*q.w-q.x*q.x+q.y*q.y-q.z*q.z, (q.y*q.z+q.x*q.w)*2}; }
static constexpr MoFloat3 qzdir(const MoFloat4 & q) { return {(q.z*q.x+q.y*q.w)*2, (q.y*q.z-q.x*q.w)*2, q.w*q.w-q.x*q.x-q.y*q.y+q.z*q.z}; }
static constexpr MoFloat4 rotation_quat(const MoFloat3 & axis, float angle)
{
    const auto a = std::sin(angle/2);
    return {axis.x*a,axis.y*a,axis.z*a,std::cos(angle/2)};
}
static constexpr MoFloat4x4 translation_matrix(const MoFloat3 & t) { return {1.f,0.f,0.f,0.f,
                                                                             0.f,1.f,0.f,0.f,
                                                                             0.f,0.f,1.f,0.f,
                                                                             t.x,t.y,t.z,1.f}; }
static constexpr MoFloat4x4 rotation_matrix(const MoFloat4 & rotation)
{
    const auto a = qxdir(rotation);
    const auto b = qydir(rotation);
    const auto c = qzdir(rotation);
    return {a.x,a.y,a.z,0.f,
            b.x,b.y,b.z,0.f,
            c.x,c.y,c.z,0.f,
            0.f,0.f,0.f,1.f};
}
static constexpr MoFloat4x4 scaling_matrix(const MoFloat3 & s) { return {s.x,0.f,0.f,0.f,
                                                                         0.f,s.y,0.f,0.f,
                                                                         0.f,0.f,s.z,0.f,
                                                                         0.f,0.f,0.f,1.f}; }
static constexpr float degreesToRadians(float angle) { return angle * 3.14159265359f / 180.0f; }

static float fastExp10(int n)
{
    switch (n)
    {
    case -9: return 0.000000001f;
    case -8: return 0.00000001f;
    case -7: return 0.0000001f;
    case -6: return 0.000001f;
    case -5: return 0.00001f;
    case -4: return 0.0001f;
    case -3: return 0.001f;
    case -2: return 0.01f;
    case -1: return 0.1f;
    case  0: return 1.f;
    case  1: return 10.f;
    case  2: return 100.f;
    case  3: return 1000.f;
    case  4: return 10000.f;
    case  5: return 100000.f;
    case  6: return 1000000.f;
    case  7: return 10000000.f;
    case  8: return 100000000.f;
    case  9: return 1000000000.f;
    default:
        return float(pow(10., n));
    }
}

static float stof(char *&p)
{
    float r = 0.0f;
    bool neg = false;
    if (*p == '-')
    {
        neg = true;
        ++p;
    }
    while (*p >= '0' && *p <= '9')
    {
        r = (r * 10.0f) + (*p - '0');
        ++p;
    }
    if (*p == '.')
    {
        float f = 0.0f;
        int n = 0;
        ++p;
        while (*p >= '0' && *p <= '9')
        {
            f = (f * 10.0f) + (*p - '0');
            ++p;
            ++n;
        }
        r += f * fastExp10(-n);
    }
    if (*p == 'e')
    {
        ++p;
        bool negExp = false;
        if (*p == '-')
        {
            negExp = true;
            ++p;
        }
        int n = 0;
        while (*p >= '0' && *p <= '9')
        {
            n = (n * 10) + (*p - '0');
            ++p;
        }
        r *= fastExp10(negExp ? -n : n);
    }
    if (neg)
    {
        r = -r;
    }
    return r;
}

static float stof(const char *p)
{
    char *t = const_cast<char *>(p);
    return stof(t);
}

static std::vector<float> stofa(const char *p, char = ' ')
{
    char *t = const_cast<char *>(p);
    std::vector<float> ret;
    while (true)
    {
        if (*t != '\0')
            ret.push_back(stof(t));
        if (*t != '\0')
            ++t;
        else
            break;
    }
    return ret;
}

static unsigned int stoui(char *&p)
{
    unsigned int r = 0U;
    while (*p >= '0' && *p <= '9')
    {
        r = (r * 10U) + unsigned(*p - '0');
        ++p;
    }
    return r;
}

static unsigned int stoui(const char *p)
{
    char *t = const_cast<char *>(p);
    return stoui(t);
}

static MoFloat2 stof2(char *&p, char = ' ')
{
    MoFloat2 ret = {};
    ret.x = stof(p); ++p;
    ret.y = stof(p);
    return ret;
}

static MoFloat2 stof2(const char *p, char = ' ')
{
    char *t = const_cast<char *>(p);
    return stof2(t);
}

static std::vector<MoFloat2> stof2a(const char *p, char = ' ', char sep2 = ' ')
{
    char *t = const_cast<char *>(p);
    std::vector<MoFloat2> ret = {};
    while (true)
    {
        ret.push_back(stof2(t, sep2));
        if (*t != '\0')
            ++t;
        else
            break;
    }
    return ret;
}

static MoFloat3 stof3(char *&p, char = ' ')
{
    MoFloat3 ret = {};
    ret.x = stof(p); ++p;
    ret.y = stof(p); ++p;
    ret.z = stof(p);
    return ret;
}

static MoFloat3 stof3(const char *p, char = ' ')
{
    char *t = const_cast<char *>(p);
    return stof3(t);
}

static std::vector<MoFloat3> stof3a(const char *p, char = ' ', char sep2 = ' ')
{
    char *t = const_cast<char *>(p);
    std::vector<MoFloat3> ret = {};
    while (true)
    {
        ret.push_back(stof3(t, sep2));
        if (*t != '\0')
            ++t;
        else
            break;
    }
    return ret;
}

static std::vector<unsigned int> stouia(const char *p, char = ' ')
{
    char *t = const_cast<char *>(p);
    std::vector<unsigned int> ret;
    while (true)
    {
        ret.push_back(stoui(t));
        if (*t != '\0')
            ++t;
        else
            break;
    }
    return ret;
}

template<typename T>
static void push_back(T **pArray, uint32_t *size, const T & value)
{
    ++(*size);
    *pArray = (T*)realloc(*pArray, sizeof(T) * nextPowerOfTwo(*size));
    (*pArray)[(*size)-1] = value;
}

template<typename T>
struct ArrayC {
    T * values;
    uint32_t size;
};

template<typename T>
static void push_back(ArrayC<T> & array, const T & value)
{
    ++(array.size);
    array.values = (T*)realloc(array.values, sizeof(T) * nextPowerOfTwo(array.size));
    array.values[array.size-1] = value;
}

static ArrayC<MoFloat2> c_stof2a(const char *p, char = ' ', char sep2 = ' ')
{
    char *t = const_cast<char *>(p);
    ArrayC<MoFloat2> ret = {};
    while (true)
    {
        push_back(ret, stof2(t, sep2));
        if (*t != '\0')
            ++t;
        else
            break;
    }
    return ret;
}

static ArrayC<MoFloat3> c_stof3a(const char *p, char = ' ', char sep2 = ' ')
{
    char *t = const_cast<char *>(p);
    ArrayC<MoFloat3> ret = {};
    while (true)
    {
        push_back(ret, stof3(t, sep2));
        if (*t != '\0')
            ++t;
        else
            break;
    }
    return ret;
}

static ArrayC<unsigned int> c_stouia(const char *p, char = ' ')
{
    char *t = const_cast<char *>(p);
    ArrayC<unsigned int> ret = {};
    while (true)
    {
        push_back(ret, stoui(t));
        if (*t != '\0')
            ++t;
        else
            break;
    }
    return ret;
}

const std::string &DAE::Effect::solve(const std::string &v) const
{
    auto sampler = std::find_if(samplers.begin(), samplers.end(), [&](const Sampler & ref) { return ref.sid == v; });
    if (sampler == samplers.end())
        return v;
    const std::string &v2 = (*sampler).source;
    auto surface = std::find_if(surfaces.begin(), surfaces.end(), [&](const Surface & ref) { return ref.sid == v2; });
    if (surface == surfaces.end())
        return v2;
    return (*surface).initFrom;
}

void moParseEffectProfilePhong(pugi::xml_node branch, DAE::Effect & effect)
{
    for (pugi::xml_node phong_value : branch)
    {
        if (strcmp(phong_value.name(), "ambient") == 0)
        {
            if (pugi::xml_node value = phong_value.child("color"))   { effect.values.push_back(DAE::Effect::Value("uniformAmbient", stofa(value.child_value()))); }
            if (pugi::xml_node value = phong_value.child("texture")) { effect.values.push_back(DAE::Effect::Value("uniformTextureAmbient", effect.solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(phong_value.name(), "diffuse") == 0)
        {
            if (pugi::xml_node value = phong_value.child("color"))   { effect.values.push_back(DAE::Effect::Value("uniformDiffuse", stofa(value.child_value()))); }
            if (pugi::xml_node value = phong_value.child("texture")) { effect.values.push_back(DAE::Effect::Value("uniformTextureDiffuse", effect.solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(phong_value.name(), "specular") == 0)
        {
            if (pugi::xml_node value = phong_value.child("color"))   { effect.values.push_back(DAE::Effect::Value("uniformSpecular", stofa(value.child_value()))); }
            if (pugi::xml_node value = phong_value.child("texture")) { effect.values.push_back(DAE::Effect::Value("uniformTextureSpecular", effect.solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(phong_value.name(), "emission") == 0)
        {
            if (pugi::xml_node value = phong_value.child("color"))   { effect.values.push_back(DAE::Effect::Value("uniformEmissive", stofa(value.child_value()))); }
            if (pugi::xml_node value = phong_value.child("texture")) { effect.values.push_back(DAE::Effect::Value("uniformTextureEmissive", effect.solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(phong_value.name(), "bump") == 0)
        {
            if (pugi::xml_node value = phong_value.child("texture")) { effect.values.push_back(DAE::Effect::Value("uniformTextureNormal", effect.solve(value.attribute("texture").as_string()))); }
        }
        else if (strcmp(phong_value.name(), "shininess") == 0)
        {
        // N/A
        }
    }
}

void moParseEffectProfile(pugi::xml_node branch, DAE::Effect & effect)
{
    for (pugi::xml_node profile_value : branch)
    {
        if (strcmp(profile_value.name(), "newparam") == 0)
        {
            if (pugi::xml_node surface = profile_value.child("surface"))
            {
                DAE::Surface reference;
                reference.sid = profile_value.attribute("sid").as_string();
                reference.initFrom = surface.child_value("init_from");
                effect.surfaces.push_back(reference);
            }
            else if (pugi::xml_node sampler2D = profile_value.child("sampler2D"))
            {
                DAE::Sampler reference;
                reference.sid = profile_value.attribute("sid").as_string();
                if ((reference.source = sampler2D.child_value("source")).empty())
                    reference.source = &sampler2D.child("instance_image").attribute("url").as_string()[1]; //#...
                effect.samplers.push_back(reference);
            }
        }
        else if (strcmp(profile_value.name(), "technique") == 0)
        {
            moParseEffectProfilePhong(profile_value.child("phong"), effect);
            if (pugi::xml_node extra = profile_value.child("extra"))
            {
                moParseEffectProfilePhong(extra.child("technique"), effect);
            }
        }
    }
}

void moParseGeometryMesh(pugi::xml_node branch, MoColladaMesh mesh)
{
    enum
    {
        kVertexIdx   = 0,
        kNormalIdx   = 1,
        kTexcoordIdx = 2
    };
    struct Input
    {
        Input() : idx(0), offset(0) {}
        unsigned int idx;
        unsigned int offset;
        std::string source;
    };

    std::vector<Input> inputs;
    pugi::xml_node polylist = branch.child("polylist");
    if (!polylist)
    {
        polylist = branch.child("triangles");
    }
    for (pugi::xml_node elem : polylist)
    {
        if (strcmp(elem.name(), "input") == 0)
        {
            Input input;
            const char * name = elem.attribute("semantic").as_string();
            if (strcmp(name, "POSITION") == 0 || strcmp(name, "VERTEX") == 0) { input.idx = kVertexIdx; }
            else if (strcmp(name, "NORMAL") == 0) { input.idx = kNormalIdx; }
            else if (strcmp(name, "TEXCOORD") == 0) { input.idx = kTexcoordIdx; }
            input.offset = elem.attribute("offset").as_uint();
            input.source = &elem.attribute("source").as_string()[1]; //#...
            inputs.push_back(input);
        }
    }
    if (pugi::xml_node vertices = branch.child("vertices"))
    {
        std::string id = vertices.attribute("id").as_string();
        auto input = std::find_if(inputs.begin(), inputs.end(), [id](const Input & input) { return id == input.source; });
        if (input != inputs.end())
        {
            (*input).source = &vertices.child("input").attribute("source").as_string()[1]; //#...
        }
    }
    for (pugi::xml_node source : branch)
    {
        if (strcmp(source.name(), "source") == 0)
        {
            std::string id = source.attribute("id").as_string();
            auto input = std::find_if(inputs.begin(), inputs.end(), [id](const Input & input) { return id == input.source; });
            if (input != inputs.end())
            {
                if ((*input).idx == kVertexIdx)
                {
                    auto vertices = c_stof3a(source.child_value("float_array"));
                    mesh->pVertices = vertices.values;
                    mesh->vertexCount = vertices.size;
                }
                else if ((*input).idx == kNormalIdx)
                {
                    auto normals = c_stof3a(source.child_value("float_array"));
                    mesh->pNormals = normals.values;
                    mesh->normalCount = normals.size;
                }
                else if ((*input).idx == kTexcoordIdx)
                {
                    auto texcoords = c_stof2a(source.child_value("float_array"));
                    mesh->pTexcoords = texcoords.values;
                    mesh->texcoordCount = texcoords.size;
                }
            }
        }
    }
    if (polylist)
    {
        std::vector<unsigned int> polygons(polylist.attribute("count").as_uint(), 3);
        if (auto data = polylist.child("vcount"))
        {
            polygons = stouia(data.child_value());
        }
        if (auto data = polylist.child("p"))
        {
            unsigned int max = 0;
            int sourceIndexes[3] = {-1,-1,-1};
            for (const auto & input : inputs)
            {
                sourceIndexes[input.idx] = int(input.offset);
                max = std::max(max, input.offset);
            }
            max += 1;
            std::vector<unsigned int> indexes = stouia(data.child_value());
            size_t i = 0;
            for (size_t j = 0; j < polygons.size(); ++j)
            {
                auto vcount = polygons[j];
                for (size_t k = 2; k < vcount; ++k)
                {
                    struct Triangle {
                        MoUInt3 vertices, texcoords, normals;
                    } face = {};
                    if (sourceIndexes[kVertexIdx] >= 0)
                    {
                        face.vertices.x = 1+indexes[i+unsigned(sourceIndexes[kVertexIdx])+(0  )*max];
                        face.vertices.y = 1+indexes[i+unsigned(sourceIndexes[kVertexIdx])+(k-1)*max];
                        face.vertices.z = 1+indexes[i+unsigned(sourceIndexes[kVertexIdx])+(k  )*max];
                    }
                    if (sourceIndexes[kNormalIdx] >= 0)
                    {
                        face.normals.x = 1+indexes[i+unsigned(sourceIndexes[kNormalIdx])+(0  )*max];
                        face.normals.y = 1+indexes[i+unsigned(sourceIndexes[kNormalIdx])+(k-1)*max];
                        face.normals.z = 1+indexes[i+unsigned(sourceIndexes[kNormalIdx])+(k  )*max];
                    }
                    if (sourceIndexes[kTexcoordIdx] >= 0)
                    {
                        face.texcoords.x = 1+indexes[i+unsigned(sourceIndexes[kTexcoordIdx])+(0  )*max];
                        face.texcoords.y = 1+indexes[i+unsigned(sourceIndexes[kTexcoordIdx])+(k-1)*max];
                        face.texcoords.z = 1+indexes[i+unsigned(sourceIndexes[kTexcoordIdx])+(k  )*max];
                    }
                    push_back((MoUInt3x3**)&mesh->pTriangles, &mesh->triangleCount, (MoUInt3x3&)face);
                }
                i += max * vcount;
            }
        }
    }
}

void moParseLibraryImages(pugi::xml_node branch, std::vector<DAE::Image> &images)
{
    for (pugi::xml_node library_image : branch)
    {
        DAE::Image image;
        image.id = library_image.attribute("id").as_string();
        if ((image.initFrom = library_image.child("init_from").child_value("ref")).empty())
            image.initFrom = library_image.child_value("init_from");
        images.push_back(image);
    }
}

void moParseLibraryEffects(pugi::xml_node branch, std::vector<DAE::Effect> &effects)
{
    for (pugi::xml_node library_effect : branch)
    {
        DAE::Effect libraryEffect;
        libraryEffect.id = library_effect.attribute("id").as_string();
        moParseEffectProfile(library_effect.child("profile_COMMON"), libraryEffect);
        effects.push_back(libraryEffect);
    }
}

void moParseLibraryMaterials(pugi::xml_node branch, MoColladaData colladaData, const DAE::LibraryData &libraryData)
{
    for (pugi::xml_node library_material : branch)
    {
        std::string libraryMaterialUrl = &library_material.child("instance_effect").attribute("url").value()[1]; //#...
        for (const auto & effect : libraryData.effects)
        {
            if (effect.id == libraryMaterialUrl)
            {
                MoColladaMaterial material = (MoColladaMaterial)malloc(sizeof(MoColladaMaterial_T));
                *material = {};
                auto name = library_material.attribute("id").as_string();
                material->name = (char*)malloc(strlen(name) + 1);
                strcpy((char*)material->name, name);

                for (const auto & value : effect.values)
                {
                    DAE::Effect::Value v = value;
                    auto image = std::find_if(libraryData.images.begin(), libraryData.images.end(), [v](const DAE::Image & image){ return image.id == v.texture; });
                    if (image != libraryData.images.end()) { v.texture = (*image).initFrom; }

                    if (v.sid == "uniformAmbient") { material->colorAmbient = {v.data[0], v.data[1], v.data[2]}; }
                    else if (v.sid == "uniformDiffuse") { material->colorDiffuse = {v.data[0], v.data[1], v.data[2]}; }
                    else if (v.sid == "uniformTextureDiffuse") { material->filenameDiffuse = (char*)malloc(v.texture.size() + 1); strcpy((char*)material->filenameDiffuse, v.texture.c_str()); }
                    else if (v.sid == "uniformSpecular") { material->colorSpecular = {v.data[0], v.data[1], v.data[2]}; }
                    else if (v.sid == "uniformTextureSpecular") { material->filenameSpecular = (char*)malloc(v.texture.size() + 1); strcpy((char*)material->filenameSpecular, v.texture.c_str()); }
                    else if (v.sid == "uniformEmissive") { material->colorEmissive = {v.data[0], v.data[1], v.data[2]}; }
                    else if (v.sid == "uniformTextureEmissive") { material->filenameEmissive = (char*)malloc(v.texture.size() + 1); strcpy((char*)material->filenameEmissive, v.texture.c_str()); }
                    else if (v.sid == "uniformTextureNormal") { material->filenameNormal = (char*)malloc(v.texture.size() + 1); strcpy((char*)material->filenameNormal, v.texture.c_str()); }
                }

                push_back((MoColladaMaterial**)&colladaData->pMaterials, &colladaData->materialCount, material);

                break;
            }
        }
    }
}

void moParseLibraryGeometries(pugi::xml_node branch, MoColladaData colladaData)
{
    for (pugi::xml_node library_geometry : branch)
    {
        MoColladaMesh mesh = (MoColladaMesh)malloc(sizeof(MoColladaMesh_T));
        *mesh = {};
        auto name = library_geometry.attribute("id").as_string();
        mesh->name = (char*)malloc(strlen(name) + 1);
        strcpy((char*)mesh->name, name);

        push_back((MoColladaMesh**)&colladaData->pMeshes, &colladaData->meshCount, mesh);
        moParseGeometryMesh(library_geometry.child("mesh"), mesh);
    }
}

void moParseLibraryVisualScenes(pugi::xml_node branch, MoColladaData collada, const std::string &upAxis)
{
    std::function<void(pugi::xml_node, MoColladaNode)> parser = [&](pugi::xml_node currentVisualNode, MoColladaNode currentNode)
    {
        currentNode->transform = { 1.f, 0.f, 0.f, 0.f,
                                   0.f, 1.f, 0.f, 0.f,
                                   0.f, 0.f, 1.f, 0.f,
                                   0.f, 0.f, 0.f, 1.f};
        for (pugi::xml_node visualNode : currentVisualNode)
        {
            if (strcmp(visualNode.name(), "instance_geometry") == 0)
            {
                auto geom_url = &visualNode.attribute("url").as_string()[1]; //#...
                for (uint32_t i = 0; i < collada->meshCount; ++i)
                {
                    if (strcmp(collada->pMeshes[i]->name, geom_url) == 0)
                    {
                        currentNode->mesh = collada->pMeshes[i];
                        break;
                    }
                }

                if (auto bind_material = visualNode.child("bind_material"))
                {
                    if (auto technique_common = bind_material.child("technique_common"))
                    {
                        if (auto instance_material = technique_common.child("instance_material"))
                        {
                            auto geom_mat = &instance_material.attribute("target").as_string()[1];
                            for (uint32_t i = 0; i < collada->materialCount; ++i)
                            {
                                if (strcmp(collada->pMaterials[i]->name, geom_mat) == 0)
                                {
                                    currentNode->material = collada->pMaterials[i];
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            else if (strcmp(visualNode.name(), "matrix") == 0)
            {
                // Collada is row major
                // we are column major
                auto m44 = stofa(visualNode.child_value());
                currentNode->transform = mul(currentNode->transform,
                                            MoFloat4x4{m44[0], m44[4], m44[8],  m44[12],
                                                       m44[1], m44[5], m44[9],  m44[13],
                                                       m44[2], m44[6], m44[10], m44[14],
                                                       m44[3], m44[7], m44[11], m44[15]});
            }
            else if (strcmp(visualNode.name(), "translate") == 0)
            {
                currentNode->transform = mul(currentNode->transform, translation_matrix(stof3(visualNode.child_value())));
            }
            else if (strcmp(visualNode.name(), "rotate") == 0)
            {
                auto v4 = stofa(visualNode.child_value()).data();
                currentNode->transform = mul(currentNode->transform, rotation_matrix(rotation_quat(MoFloat3{v4[0], v4[1], v4[2]}, degreesToRadians(v4[3]))));
            }
            else if (strcmp(visualNode.name(), "scale") == 0)
            {
                currentNode->transform = mul(currentNode->transform, scaling_matrix(stof3(visualNode.child_value())));
            }
        }

        for (pugi::xml_node visualNode : currentVisualNode)
        {
            if (strcmp(visualNode.name(), "node") == 0)
            {
                MoColladaNode node = (MoColladaNode)malloc(sizeof(MoColladaNode_T));
                *node = {};

                auto name = visualNode.attribute("name").as_string();
                node->name = (char*)malloc(strlen(name) + 1);
                strcpy((char*)node->name, name);

                push_back((MoColladaNode**)&currentNode->pNodes, &currentNode->nodeCount, node);
                parser(visualNode, node);
            }
        }

        if (upAxis == "Z_UP")
        {
            currentNode->transform = mul(MoFloat4x4{1.f, 0.f, 0.f, 0.f,
                                                    0.f, 0.f,-1.f, 0.f,
                                                    0.f, 1.f, 0.f, 0.f,
                                                    0.f, 0.f, 0.f, 1.f}, currentNode->transform);
        }
        else if (upAxis == "X_UP")
        {
            currentNode->transform = mul(MoFloat4x4{ 0.f, 1.f, 0.f, 0.f,
                                                    -1.f, 0.f, 0.f, 0.f,
                                                     0.f, 0.f, 1.f, 0.f,
                                                     0.f, 0.f, 0.f, 1.f}, currentNode->transform);
        }
    };

    for (pugi::xml_node library_visual_scene : branch)
    {
        for (pugi::xml_node visualNode : library_visual_scene)
        {
            if (strcmp(visualNode.name(), "node") == 0)
            {
                MoColladaNode node = (MoColladaNode)malloc(sizeof(MoColladaNode_T));
                *node = {};

                auto name = visualNode.attribute("name").as_string();
                node->name = (char*)malloc(strlen(name) + 1);
                strcpy((char*)node->name, name);

                push_back((MoColladaNode**)&collada->pNodes, &collada->nodeCount, node);
                parser(visualNode, node);
            }
        }
    }
}

void moParseCollada(pugi::xml_node root, DAE::LibraryData &libraryData)
{
    moParseLibraryImages(root.child("library_images"), libraryData.images);
    moParseLibraryEffects(root.child("library_effects"), libraryData.effects);
}

void moCreateColladaData(MoColladaDataCreateInfo *pCreateInfo, MoColladaData *pColladaData)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(pCreateInfo->pContents);
    assert(result.status == pugi::status_ok);

    auto root = doc.child("COLLADA");
    std::string version = root.attribute("version").as_string();
    printf("COLLADA version '%s'\n", version.c_str());
    DAE::LibraryData libraryData;
    libraryData.upAxis = root.child("asset").child_value("up_axis");
    moParseCollada(root, libraryData);

    MoColladaData colladaData = *pColladaData = (MoColladaData)malloc(sizeof(MoColladaData_T));
    *colladaData = {};
    moParseLibraryGeometries(root.child("library_geometries"), colladaData);
    moParseLibraryMaterials(root.child("library_materials"), colladaData, libraryData);
    moParseLibraryVisualScenes(root.child("library_visual_scenes"), colladaData, libraryData.upAxis);
}

void moDestroyColladaNode(MoColladaNode node)
{
    free((char*)node->name);
    for (uint32_t i = 0; i < node->nodeCount; ++i) { moDestroyColladaNode(node->pNodes[i]); }
    free((MoColladaNode*)node->pNodes);
    free(node);
}

void moDestroyColladaMesh(MoColladaMesh mesh)
{
    free((char*)mesh->name);
    free((MoUInt3x3*)mesh->pTriangles);
    free((MoFloat3*)mesh->pVertices);
    free((MoFloat2*)mesh->pTexcoords);
    free((MoFloat3*)mesh->pNormals);
    free(mesh);
}

void moDestroyColladaMaterial(MoColladaMaterial material)
{
    free((char*)material->name);
    free((char*)material->filenameDiffuse);
    free((char*)material->filenameNormal);
    free((char*)material->filenameSpecular);
    free((char*)material->filenameEmissive);
    free(material);
}

void moDestroyColladaData(MoColladaData collada)
{
    for (uint32_t i = 0; i < collada->nodeCount; ++i) { moDestroyColladaNode(collada->pNodes[i]); }
    for (uint32_t i = 0; i < collada->meshCount; ++i) { moDestroyColladaMesh(collada->pMeshes[i]); }
    for (uint32_t i = 0; i < collada->materialCount; ++i) { moDestroyColladaMaterial(collada->pMaterials[i]); }
    free((MoColladaNode*)collada->pNodes);
    free((MoColladaMesh*)collada->pMeshes);
    free((MoColladaMaterial*)collada->pMaterials);
    free(collada);
}

void moTestCollada()
{
    static const char *data =
"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
"<COLLADA xmlns=\"http://www.collada.org/2005/11/COLLADASchema\" version=\"1.4.1\">\n"
"  <asset>\n"
"    <contributor>\n"
"      <author>Blender User</author>\n"
"      <authoring_tool>Blender 2.79.0</authoring_tool>\n"
"    </contributor>\n"
"    <created>2019-01-06T10:17:21</created>\n"
"    <modified>2019-01-06T10:17:21</modified>\n"
"    <unit name=\"meter\" meter=\"1\"/>\n"
"    <up_axis>Z_UP</up_axis>\n"
"  </asset>\n"
"  <library_images/>\n"
"  <library_geometries>\n"
"    <geometry id=\"Cube-mesh\" name=\"Cube\">\n"
"      <mesh>\n"
"        <source id=\"Cube-mesh-positions\">\n"
"          <float_array id=\"Cube-mesh-positions-array\" count=\"24\">-1 -1 -1 -1 -1 1 -1 1 -1 -1 1 1 1 -1 -1 1 -1 1 1 1 -1 1 1 1</float_array>\n"
"          <technique_common>\n"
"            <accessor source=\"#Cube-mesh-positions-array\" count=\"8\" stride=\"3\">\n"
"              <param name=\"X\" type=\"float\"/>\n"
"              <param name=\"Y\" type=\"float\"/>\n"
"              <param name=\"Z\" type=\"float\"/>\n"
"            </accessor>\n"
"          </technique_common>\n"
"        </source>\n"
"        <source id=\"Cube-mesh-normals\">\n"
"          <float_array id=\"Cube-mesh-normals-array\" count=\"18\">-1 0 0 0 1 0 1 0 0 0 -1 0 0 0 -1 0 0 1</float_array>\n"
"          <technique_common>\n"
"            <accessor source=\"#Cube-mesh-normals-array\" count=\"6\" stride=\"3\">\n"
"              <param name=\"X\" type=\"float\"/>\n"
"              <param name=\"Y\" type=\"float\"/>\n"
"              <param name=\"Z\" type=\"float\"/>\n"
"            </accessor>\n"
"          </technique_common>\n"
"        </source>\n"
"        <source id=\"Cube-mesh-map-0\">\n"
"          <float_array id=\"Cube-mesh-map-0-array\" count=\"72\">0.3333334 0.6666666 0.6666667 0.3333333 0.6666668 0.6666666 0 0.6666667 0.3333333 "
"0.3333333 0.3333333 0.6666666 1.29143e-7 0.3333333 0.3333333 0 0.3333334 0.3333333 0.3333334 0 0.6666668 0.3333333 0.3333334 0.3333333 0.6666668 0.3333332 "
"1 0 1 0.3333332 0.3333333 1 0 0.6666668 0.3333333 0.6666667 0.3333334 0.6666666 0.3333334 0.3333334 0.6666667 0.3333333 0 0.6666667 0 0.3333334 0.3333333 "
"0.3333333 1.29143e-7 0.3333333 0 0 0.3333333 0 0.3333334 0 0.6666667 0 0.6666668 0.3333333 0.6666668 0.3333332 0.6666668 0 1 0 0.3333333 1 0 1 0 0.6666668</float_array>\n"
"          <technique_common>\n"
"            <accessor source=\"#Cube-mesh-map-0-array\" count=\"36\" stride=\"2\">\n"
"              <param name=\"S\" type=\"float\"/>\n"
"              <param name=\"T\" type=\"float\"/>\n"
"            </accessor>\n"
"          </technique_common>\n"
"        </source>\n"
"        <vertices id=\"Cube-mesh-vertices\">\n"
"          <input semantic=\"POSITION\" source=\"#Cube-mesh-positions\"/>\n"
"        </vertices>\n"
"        <triangles count=\"12\">\n"
"          <input semantic=\"VERTEX\" source=\"#Cube-mesh-vertices\" offset=\"0\"/>\n"
"          <input semantic=\"NORMAL\" source=\"#Cube-mesh-normals\" offset=\"1\"/>\n"
"          <input semantic=\"TEXCOORD\" source=\"#Cube-mesh-map-0\" offset=\"2\" set=\"0\"/>\n"
"          <p>1 0 0 2 0 1 0 0 2 3 1 3 6 1 4 2 1 5 7 2 6 4 2 7 6 2 8 5 3 9 0 3 10 4 3 11 6 4 12 0 4 13 2 4 14 3 5 15 5 5 16 7 5 17 1 0 18 3 0 19 2 0 20 3 1 21 "
"7 1 22 6 1 23 7 2 24 5 2 25 4 2 26 5 3 27 1 3 28 0 3 29 6 4 30 4 4 31 0 4 32 3 5 33 1 5 34 5 5 35</p>\n"
"        </triangles>\n"
"      </mesh>\n"
"    </geometry>\n"
"  </library_geometries>\n"
"  <library_controllers/>\n"
"  <library_visual_scenes>\n"
"    <visual_scene id=\"Scene\" name=\"Scene\">\n"
"      <node id=\"Node\" name=\"Node\" type=\"NODE\">\n"
"        <matrix sid=\"transform\">7.54979e-8 -1 0 0 1 7.54979e-8 0 0 0 0 1 0 0 0 0 1</matrix>\n"
"        <node id=\"Cube\" name=\"Cube\" type=\"NODE\">\n"
"          <matrix sid=\"transform\">1 0 0 0 0 1 0 -4 0 0 1 2 0 0 0 1</matrix>\n"
"          <instance_geometry url=\"#Cube-mesh\" name=\"Cube\"/>\n"
"        </node>\n"
"      </node>\n"
"    </visual_scene>\n"
"  </library_visual_scenes>\n"
"  <scene>\n"
"    <instance_visual_scene url=\"#Scene\"/>\n"
"  </scene>\n"
"</COLLADA>\n";

    MoColladaDataCreateInfo createInfo;
    createInfo.pContents = data;

    MoColladaData collada;
    moCreateColladaData(&createInfo, &collada);

    assert(       collada->materialCount == 0);
    assert(       collada->meshCount == 1);
    assert(strcmp(collada->pMeshes[0]->name, "Cube-mesh") == 0);
    assert(       collada->nodeCount == 1);
    assert(strcmp(collada->pNodes[0]->name, "Node") == 0);
    assert(       collada->pNodes[0]->nodeCount == 1);
    assert(strcmp(collada->pNodes[0]->pNodes[0]->name, "Cube") == 0);
    assert(       collada->pNodes[0]->pNodes[0]->nodeCount == 0);
    assert(       collada->pNodes[0]->pNodes[0]->mesh == collada->pMeshes[0]);

    moDestroyColladaData(collada);
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
