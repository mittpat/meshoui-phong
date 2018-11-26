#pragma once

#include "enums.h"
#include <hashid.h>
#include <linalg.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Meshoui
{
    class Program;
    class RendererPrivate;
    class Mesh
    {
    public:
        virtual ~Mesh();
        Mesh();

        linalg::aliases::float4x4 modelMatrix() const;
        RendererPrivate * d_ptr() const;

        HashId instanceId;
        HashId definitionId;
        std::string filename;
        Program * program;

        linalg::aliases::float3 scale;
        linalg::aliases::float4 orientation;
        linalg::aliases::float3 position;

        View::Flags viewFlags;
        Render::Flags renderFlags;

        std::vector<linalg::aliases::float4x4> collisions;

    private:
        friend class RendererPrivate;
        RendererPrivate * d;
    };
    inline Mesh::Mesh() : program(nullptr), scale(1.f, 1.f, 1.f), orientation(linalg::identity), position(0.f, 0.f, 0.f), viewFlags(View::All), renderFlags(Render::Default) {}
    inline RendererPrivate *Mesh::d_ptr() const { return d; }

    // Mesh factory
    class Model
    {
    public:
        virtual ~Model();
        Model();
        Model(const std::string & f);

        template<typename T>
        std::vector<T *> meshFactory(linalg::aliases::float3 position = linalg::zero) const;
        size_t meshCount() const;
        RendererPrivate * d_ptr() const;

        std::string filename;

    private:
        void fill(const std::vector<Mesh *> &meshes) const;

        friend class RendererPrivate;
        RendererPrivate * d;
    };
    inline Model::~Model() {}
    inline Model::Model(): d(nullptr) {}
    inline Model::Model(const std::string & f) : filename(f), d(nullptr) {}
    template<typename T> inline std::vector<T *> Model::meshFactory(linalg::aliases::float3 position) const
    {
        size_t count = meshCount();
        std::vector<T *> meshes; meshes.reserve(count);
        for (size_t i = 0; i < count; ++i) meshes.emplace_back(new T());
        fill(std::vector<Mesh *>(meshes.begin(), meshes.end()));
        for (size_t i = 0; i < count; ++i) meshes[i]->position += position;
        return meshes;
    }
    inline RendererPrivate *Model::d_ptr() const { return d; }
}
