#include <GL/glew.h>

#include "GraphicsUniform.h"

IGraphicsUniform *GraphicsUniformFactory::makeUniform(HashId name, GLenum type, int size)
{
    IGraphicsUniform * uniform = nullptr;
    switch (type)
    {
    //case GL_FLOAT:
    //    uniform = new GraphicsUniform1fv(name);
    //    break;
    case GL_FLOAT_VEC2_ARB:
        uniform = new GraphicsUniform2fv(name);
        break;
    case GL_FLOAT_VEC3_ARB:
        uniform = new GraphicsUniform3fv(name);
        break;
    case GL_FLOAT_VEC4_ARB:
        uniform = new GraphicsUniform4fv(name);
        break;
    case GL_FLOAT_MAT2_ARB:
        uniform = new GraphicsUniform22fm(name);
        break;
    case GL_FLOAT_MAT3_ARB:
        uniform = new GraphicsUniform33fm(name);
        break;
    case GL_FLOAT_MAT4_ARB:
        uniform = new GraphicsUniform44fm(name);
        break;
    case GL_SAMPLER_2D_ARB:
        uniform = new GraphicsUniformSampler2D(name);
    case GL_INT:
        switch (size) {
        case 16: uniform = new GraphicsUniformMiv<16>(name); break;
        default:
            break;
        }
    default:
        break;
    }
    return uniform;
}
