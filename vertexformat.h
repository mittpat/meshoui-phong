#pragma once

#include <vulkan/vulkan.h>

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

// vertex type, can be declared as your own so long as memory alignment is respected
#if !defined(MO_SKIP_VERTEX_TYPE) && !defined(MO_VERTEX_TYPE_DEFINED)
#define MO_VERTEX_TYPE_DEFINED
typedef union MoVertex {
    struct {
        MoFloat3 position;
        MoFloat2 texcoord;
        MoFloat3 normal;
        MoFloat3 tangent;
        MoFloat3 bitangent;
    };
    float data[14];
} MoVertex;
#endif

typedef struct MoVertexAttribute {
    const float* pAttribute;
    uint32_t     attributeCount;
    uint32_t     componentCount;
} MoVertexAttribute;

typedef enum MoVertexFormatCreateFlagBits {
    MO_VERTEX_FORMAT_INDICES_COUNT_FROM_ONE_BIT = 0x00000001,
    MO_VERTEX_FORMAT_INDICES_PER_ATTRIBUTE = 0x00000002,
    MO_VERTEX_FORMAT_DISABLE_REINDEXING_BIT = 0x00000004,
    MO_VERTEX_FORMAT_DISCARD_NORMALS_BIT = 0x00000008,
    MO_VERTEX_FORMAT_GENERATE_TANGENTS_BIT = 0x00000010,
    MO_VERTEX_FORMAT_REWIND_INDICES_BIT = 0x00000020,
    MO_VERTEX_FORMAT_INDICES_MAX_ENUM = 0x7FFFFFFF
} MoVertexFormatCreateFlagBits;
typedef VkFlags MoVertexFormatCreateFlags;

typedef struct MoVertexFormatCreateInfo {
    const MoVertexAttribute*  pAttributes;
    uint32_t                  attributeCount;
    const uint8_t*            pIndices;
    uint32_t                  indexCount;
    uint32_t                  indexTypeSize;
    MoVertexFormatCreateFlags flags;
} MoVertexFormatCreateInfo;

typedef struct MoVertexFormat_T {
    const uint32_t* pIndices;
    uint32_t        indexCount;
    const MoVertex* pVertices;
    uint32_t        vertexCount;
}* MoVertexFormat;

// converts from collada to MoVertex
void moCreateVertexFormat(MoVertexFormatCreateInfo* pCreateInfo, MoVertexFormat* pFormat);

// free vertex format
void moDestroyVertexFormat(MoVertexFormat format);

// test
void moTestVertexFormat();

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
