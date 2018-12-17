#pragma once

#include "enums.h"
#include "hashid.h"
#include <linalg.h>

#include <algorithm>
#include <string>
#include <vector>

namespace Meshoui
{
    class Program;
    class MaterialPrivate;
    class MeshPrivate;
    class Mesh
    {
    public:
        virtual ~Mesh();
        Mesh();

        Program * program;

        HashId instanceId;
        HashId definitionId;
        HashId materialId;
        std::string filename;

        linalg::aliases::float4x4 modelMatrix;

        View::Flags viewFlags;
        Render::Flags renderFlags;

    private:
        friend class RendererPrivate;
        MeshPrivate * d;
        MaterialPrivate * m;
    };
    inline Mesh::Mesh() : program(nullptr), modelMatrix(linalg::identity), viewFlags(View::All), renderFlags(Render::Default), d(nullptr), m(nullptr) {}

    // Mesh factory
    class RendererPrivate;
    class Model
    {
    public:
        virtual ~Model();
        Model();
        Model(const std::string & f);

        template<typename T>
        std::vector<T *> meshFactory(linalg::aliases::float3 position = linalg::zero) const;
        size_t meshCount() const;

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
        for (size_t i = 0; i < count; ++i) meshes[i]->modelMatrix.w += linalg::aliases::float4(position, 0.0f);
        return meshes;
    }
}
