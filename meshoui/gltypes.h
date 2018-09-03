#pragma once

#include <stddef.h>

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef signed char GLbyte;
typedef short GLshort;
typedef int GLint;
typedef int GLsizei;
typedef unsigned char GLubyte;
typedef unsigned short GLushort;
typedef unsigned int GLuint;
typedef float GLfloat;
typedef float GLclampf;
typedef void GLvoid;
typedef int GLfixed;
typedef int GLclampx;

#define GL_NONE 0
#define GL_FLOAT_VEC2_ARB 0x8B50
#define GL_FLOAT_VEC3_ARB 0x8B51
#define GL_FLOAT_VEC4_ARB 0x8B52
#define GL_SAMPLER_2D_ARB 0x8B5E

inline constexpr GLenum enumForVectorSize(size_t size)
{
    switch (size)
    {
    case 2: return GL_FLOAT_VEC2_ARB;
    case 3: return GL_FLOAT_VEC3_ARB;
    case 4: return GL_FLOAT_VEC4_ARB;
    default:
        return GL_SAMPLER_2D_ARB; //Maybe???
    }
    return GL_NONE;
}
