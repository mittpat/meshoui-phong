#include <GL/glew.h>

#include "Program.h"
#include "RendererPrivate.h"
#include "Uniform.h"

#include <loose.h>
#include <iniparser.h>

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

namespace
{
    void readUniform(const std::string & filename, const std::string & name, const std::string & value, Program * program)
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
            auto uniform = UniformFactory::makeUniform(name, enumForVectorSize(values.size()));
            uniform->setData(values.data());
            if (auto sampler = dynamic_cast<UniformSampler2D *>(uniform))
            {
                sampler->filename = sibling(value, filename);
            }
            program->add(uniform);
        }
    }
}

Program::~Program()
{
    for (auto uniform : uniforms)
    {
        delete uniform;
    }
}

void Program::load(const std::string & filename)
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
    defines.push_back(std::string("#version") + " " + iniparser_getstring(ini, "defines:version", "150") + "\n");
    std::vector<const char*> defKeys(iniparser_getsecnkeys(ini, "defines"), nullptr);
    if (iniparser_getseckeys(ini, "defines", defKeys.data()) != nullptr)
    {
        for (const char * key : defKeys)
        {
            if (strcmp(key, "defines:version") == 0)
                continue;
            else
                defines.push_back(std::string("#define") + " " + split(key, ':')[1] + " " + iniparser_getstring(ini, key, "") + "\n");
        }
    }
    iniparser_freedict(ini);
}

void Program::add(IUniform * uniform)
{
    if (std::find(uniforms.begin(), uniforms.end(), uniform) == uniforms.end())
    {
        uniforms.push_back(uniform);
    }
}

void Program::remove(IUniform * uniform)
{
    uniforms.erase(std::remove(uniforms.begin(), uniforms.end(), uniform));
}

void Program::applyUniforms()
{
    for (auto* uniform : uniforms)
    {
        d_ptr()->setProgramUniform(this, uniform);
    }
}

void Program::unapplyUniforms()
{
    for (auto* uniform : uniforms)
    {
        d_ptr()->unsetProgramUniform(this, uniform);
    }
}

void Program::draw(Mesh * mesh)
{
    d_ptr()->draw(this, mesh);
}

IUniform *Program::uniform(HashId name) const
{
    auto found = std::find_if(uniforms.begin(), uniforms.end(), [name](IUniform * uniform){
        return uniform->name == name;
    });
    if (found != uniforms.end())
        return *found;
    return nullptr;
}
