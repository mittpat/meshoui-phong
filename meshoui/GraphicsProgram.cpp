#include <GL/glew.h>

#include "GraphicsProgram.h"
#include "GraphicsPrivate.h"
#include "GraphicsUniform.h"

#include "assert.h"
#include "loose.h"
#include <iniparser.h>

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace linalg;
using namespace linalg::aliases;

namespace
{
    void readUniform(const std::string & filename, const std::string & name, const std::string & value, GraphicsProgram * program)
    {
        std::vector<GLfloat> values;
        std::string s;
        std::istringstream valueSplitter(value);
        while (std::getline(valueSplitter, s, ' '))
        {
            try { values.push_back(conv::stof(s.c_str())); }
            catch (std::invalid_argument) {}
        }
        if (!name.empty())
        {
            auto uniform = GraphicsUniformFactory::makeUniform(name, enumForVectorSize(values.size()));
            uniform->setData(values.data());
            if (auto sampler = dynamic_cast<GraphicsUniformSampler2D *>(uniform))
            {
                sampler->filename = sibling(value, filename);
            }
            program->add(uniform);
        }
    }
}

GraphicsProgram::~GraphicsProgram()
{
    for (auto uniform : uniforms)
    {
        delete uniform;
    }
}

void GraphicsProgram::load(const std::string & filename)
{
    dictionary * ini = iniparser_load(filename.c_str());
    {
        std::string shaderFilename = iniparser_getstring(ini, "vertexShader:filename", "");
        std::ifstream fileStream(sibling(shaderFilename, filename));
        vertexShaderSource = std::string((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    {
        std::string shaderFilename = iniparser_getstring(ini, "fragmentShader:filename", "");
        std::ifstream fileStream(sibling(shaderFilename, filename));
        fragmentShaderSource = std::string((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    std::vector<const char*> keys(iniparser_getsecnkeys(ini, "uniforms"), nullptr);
    if (iniparser_getseckeys(ini, "uniforms", keys.data()) != nullptr)
    {
        for (const char * key : keys)
        {
            std::string line = iniparser_getstring(ini, key, "");
            readUniform(filename, split(key, ':')[1], line, this);
        }
    }
    iniparser_freedict(ini);
}

void GraphicsProgram::add(IGraphicsUniform * uniform)
{
    if (std::find(uniforms.begin(), uniforms.end(), uniform) == uniforms.end())
    {
        uniforms.push_back(uniform);
    }
}

void GraphicsProgram::remove(IGraphicsUniform * uniform)
{
    uniforms.erase(std::remove(uniforms.begin(), uniforms.end(), uniform));
}

void GraphicsProgram::applyUniforms()
{
    for (auto* uniform : uniforms)
    {
        d_ptr()->setProgramUniform(this, uniform);
    }
}

void GraphicsProgram::unapplyUniforms()
{
    for (auto* uniform : uniforms)
    {
        d_ptr()->unsetProgramUniform(this, uniform);
    }
}

void GraphicsProgram::draw(Mesh * mesh)
{
    d_ptr()->draw(this, mesh);
}

IGraphicsUniform *GraphicsProgram::uniform(HashId name) const
{
    auto found = std::find_if(uniforms.begin(), uniforms.end(), [name](IGraphicsUniform * uniform){
        return uniform->name == name;
    });
    if (found != uniforms.end())
        return *found;
    return nullptr;
}
