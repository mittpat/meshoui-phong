#include <GL/glew.h>

#include "Program.h"
#include "RendererPrivate.h"

#include <loose.h>
#include <iniparser.h>

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace linalg;
using namespace linalg::aliases;
using namespace Meshoui;

Program::~Program()
{

}

void Program::load(const std::string & filename)
{
    dictionary * ini = iniparser_load(filename.c_str());
    {
        std::string shaderFilename = iniparser_getstring(ini, "vertexShader:filename", "");
        std::ifstream fileStream(sibling(shaderFilename, filename));
        vertexShaderSource = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    {
        std::string shaderFilename = iniparser_getstring(ini, "fragmentShader:filename", "");
        std::ifstream fileStream(sibling(shaderFilename, filename));
        fragmentShaderSource = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    }
    iniparser_freedict(ini);
}

void Program::draw(Mesh * mesh)
{
    d_ptr()->draw(this, mesh);
}
