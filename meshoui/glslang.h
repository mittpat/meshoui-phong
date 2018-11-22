#pragma once

#include <cstdint>
#include <hashid.h>
#include <loose.h>
#include <sstream>
#include <vector>

class ReflectionInfo final
{
public:
    ~ReflectionInfo();
    ReflectionInfo();

    HashId name;
    int32_t offset;
    uint32_t type;
    uint32_t size;
    uint32_t index;
    int32_t binding;
    int32_t stages;
};
inline ReflectionInfo::~ReflectionInfo() {}
inline ReflectionInfo::ReflectionInfo() : offset(0), type(0U), size(0U), index(0U), binding(0), stages(0) {}

class ShaderModuleReflectionInfo final
{
public:
    ~ShaderModuleReflectionInfo();
    ShaderModuleReflectionInfo();

    std::vector<ReflectionInfo> uniformReflection;
    std::vector<ReflectionInfo> uniformBlockReflection;
    std::vector<ReflectionInfo> vertexAttributeReflection;
};
inline ShaderModuleReflectionInfo::~ShaderModuleReflectionInfo() {}
inline ShaderModuleReflectionInfo::ShaderModuleReflectionInfo() {}

class PipelineReflectionInfo final
{
public:
    ~PipelineReflectionInfo();
    PipelineReflectionInfo();

    ShaderModuleReflectionInfo vertexShaderStage;
    ShaderModuleReflectionInfo fragmentShaderStage;
};
inline PipelineReflectionInfo::~PipelineReflectionInfo() {}
inline PipelineReflectionInfo::PipelineReflectionInfo() {}

void glslLangOutputParse(ShaderModuleReflectionInfo &shaderModuleReflectionInfo, const std::string &output);
