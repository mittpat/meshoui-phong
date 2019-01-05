#pragma once

#include <cstdint>

// vector types, can be declared as your own so long as memory alignment is respected
#if !defined(MO_SKIP_VEC_TYPES) && !defined(MO_VEC_TYPES_DEFINED)
#define MO_VEC_TYPES_DEFINED
typedef union MoUInt3 {
    struct {
        uint32_t x;
        uint32_t y;
        uint32_t z;
    };
    uint32_t data[3];
} MoUInt3;

typedef union MoUInt3x3 {
    struct {
        MoUInt3 x;
        MoUInt3 y;
        MoUInt3 z;
    };
    uint32_t data[9];
} MoUInt3x3;

typedef union MoFloat2 {
    struct {
        float x;
        float y;
    };
    float data[2];
} MoFloat2;

typedef union MoFloat3 {
    struct {
        float x;
        float y;
        float z;
    };
    float data[3];
} MoFloat3;

typedef union MoFloat3x3 {
    struct {
        MoFloat3 x;
        MoFloat3 y;
        MoFloat3 z;
    };
    float data[9];
} MoFloat3x3;

typedef union MoFloat4 {
    struct {
        float x;
        float y;
        float z;
        float w;
    };
    float data[4];
} MoFloat4;

typedef union MoFloat4x4 {
    struct {
        MoFloat4 x;
        MoFloat4 y;
        MoFloat4 z;
        MoFloat4 w;
    };
    float data[16];
} MoFloat4x4;
#endif

typedef struct MoColladaMesh_T {
    const char*      name;
    const MoUInt3x3* pTriangles;
    uint32_t         triangleCount;
    const MoFloat3*  pVertices;
    uint32_t         vertexCount;
    const MoFloat2*  pTexcoords;
    uint32_t         texcoordCount;
    const MoFloat3*  pNormals;
    uint32_t         normalCount;
    void*            userData;
}* MoColladaMesh;

typedef struct MoColladaMaterial_T {
    const char* name;
    MoFloat3    colorAmbient;
    MoFloat3    colorDiffuse;
    MoFloat3    colorSpecular;
    MoFloat3    colorEmissive;
    const char* filenameDiffuse;
    const char* filenameNormal;
    const char* filenameSpecular;
    const char* filenameEmissive;
}* MoColladaMaterial;

typedef struct MoColladaNode_T* MoColladaNode;
struct MoColladaNode_T {
    const char*          name;
    MoFloat4x4           transform;
    MoColladaMesh        mesh;
    MoColladaMaterial    material;
    const MoColladaNode* pNodes;
    uint32_t             nodeCount;
};

typedef struct MoColladaData_T {
    const MoColladaNode*     pNodes;
    uint32_t                 nodeCount;
    const MoColladaMesh*     pMeshes;
    uint32_t                 meshCount;
    const MoColladaMaterial* pMaterials;
    uint32_t                 materialCount;
}* MoColladaData;

// currently unused
typedef enum MoColladaDataCreateFlagBits {
    MO_COLLADA_DATA_GRAPHICS_BIT = 0x00000001,
    MO_COLLADA_DATA_PHYSICS_BIT = 0x00000002,
    MO_COLLADA_DATA_MAX_ENUM = 0x7FFFFFFF
} MoColladaDataCreateFlagBits;
typedef uint32_t MoColladaDataCreateFlags;

typedef struct MoColladaDataCreateInfo {
    const char*              pContents;
} MoColladaDataCreateInfo;

// parse a collada document to nodes, meshes and materials
void moCreateColladaData(MoColladaDataCreateInfo* pCreateInfo, MoColladaData* pColladaData);

// destroy parsed collada document's nodes, meshes and materials
void moDestroyColladaData(MoColladaData collada);

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
