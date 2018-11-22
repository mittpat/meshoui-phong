#include "glslang.h"

void glslLangOutputParse(ShaderModuleReflectionInfo &shaderModuleReflectionInfo, const std::string &output)
{
    enum Section
    {
        Invalid,
        Uniform_reflection,
        Uniform_block_reflection,
        Vertex_attribute_reflection
    } currentSection = Invalid;

    std::istringstream input(output.data());
    for (std::string line; std::getline(input, line); )
    {
        if      (line == "Uniform reflection:")          { currentSection = Uniform_reflection;          }
        else if (line == "Uniform block reflection:")    { currentSection = Uniform_block_reflection;    }
        else if (line == "Vertex attribute reflection:") { currentSection = Vertex_attribute_reflection; }
        else
        {
            auto uni = split(line, ':');
            if (uni.size() > 1)
            {
                switch (currentSection)
                {
                case Uniform_reflection:
                {
                    ReflectionInfo uniform;
                    uniform.name = uni[0];
                    for (auto entry : split(uni[1], ','))
                    {
                        auto pair = split(entry, ' ');
                        if      (pair[0] == "offset")  uniform.offset  = std::stoi(pair[1]);
                        else if (pair[0] == "type")    uniform.type    = std::stoul(pair[1], nullptr, 16);
                        else if (pair[0] == "size")    uniform.size    = std::stoul(pair[1]);
                        else if (pair[0] == "index")   uniform.index   = std::stoul(pair[1]);
                        else if (pair[0] == "binding") uniform.binding = std::stoi(pair[1]);
                        else if (pair[0] == "stages")  uniform.stages  = std::stoi(pair[1]);
                    }
                    shaderModuleReflectionInfo.uniformReflection.push_back(uniform);
                    break;
                }
                case Uniform_block_reflection:
                {
                    ReflectionInfo block;
                    block.name = uni[0];
                    for (auto entry : split(uni[1], ','))
                    {
                        auto pair = split(entry, ' ');
                        if      (pair[0] == "offset")  block.offset  = std::stoi(pair[1]);
                        else if (pair[0] == "type")    block.type    = std::stoul(pair[1], nullptr, 16);
                        else if (pair[0] == "size")    block.size    = std::stoul(pair[1]);
                        else if (pair[0] == "index")   block.index   = std::stoul(pair[1]);
                        else if (pair[0] == "binding") block.binding = std::stoi(pair[1]);
                        else if (pair[0] == "stages")  block.stages  = std::stoi(pair[1]);
                    }
                    shaderModuleReflectionInfo.uniformBlockReflection.push_back(block);
                    break;
                }
                case Vertex_attribute_reflection:
                {
                    ReflectionInfo attribute;
                    attribute.name = uni[0];
                    for (auto entry : split(uni[1], ','))
                    {
                        auto pair = split(entry, ' ');
                        if      (pair[0] == "offset")  attribute.offset  = std::stoi(pair[1]);
                        else if (pair[0] == "type")    attribute.type    = std::stoul(pair[1], nullptr, 16);
                        else if (pair[0] == "size")    attribute.size    = std::stoul(pair[1]);
                        else if (pair[0] == "index")   attribute.index   = std::stoul(pair[1]);
                        else if (pair[0] == "binding") attribute.binding = std::stoi(pair[1]);
                        else if (pair[0] == "stages")  attribute.stages  = std::stoi(pair[1]);
                    }
                    shaderModuleReflectionInfo.vertexAttributeReflection.push_back(attribute);
                    break;
                }
                default:
                    break;
                }
            }
        }
    }
}
