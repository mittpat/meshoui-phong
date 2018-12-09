#include "Program.h"

#include <loose.h>
#include <fstream>

using namespace Meshoui;

void Program::load(const std::string & filename)
{
    std::ifstream shaderFileStream(filename);
    for (std::string line; std::getline(shaderFileStream, line); )
    {
        auto pair = split(line, '=');
        if (pair.size() == 2)
        {
            if (pair[0] == "vertexShaderFilename")
            {
                std::ifstream fileStream(sibling(pair[1], filename), std::ifstream::binary);
                vertexShaderSource = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
            }
            if (pair[0] == "fragmentShaderFilename")
            {
                std::ifstream fileStream(sibling(pair[1], filename), std::ifstream::binary);
                fragmentShaderSource = std::vector<char>((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
            }
        }
    }
}
