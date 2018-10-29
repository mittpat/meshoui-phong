#include <GL/glew.h>

#include "Uniform.h"

using namespace Meshoui;

IUniform *UniformFactory::makeUniform(HashId name, GLenum type, int size)
{
    IUniform * uniform = nullptr;
    switch (type)
    {
    //case GL_FLOAT:
    //    uniform = new GraphicsUniform1fv(name);
    //    break;
    case GL_FLOAT_VEC2_ARB:
        uniform = new Uniform2fv(name);
        break;
    case GL_FLOAT_VEC3_ARB:
        uniform = new Uniform3fv(name);
        break;
    case GL_FLOAT_VEC4_ARB:
        uniform = new Uniform4fv(name);
        break;
    case GL_FLOAT_MAT2_ARB:
        uniform = new Uniform22fm(name);
        break;
    case GL_FLOAT_MAT3_ARB:
        uniform = new Uniform33fm(name);
        break;
    case GL_FLOAT_MAT4_ARB:
        uniform = new Uniform44fm(name);
        break;
    case GL_SAMPLER_2D_ARB:
        uniform = new UniformSampler2D(name);
        break;
    case GL_INT:
        switch (size)
        {
        case 16:
            uniform = new UniformMiv<16>(name);
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
    return uniform;
}
