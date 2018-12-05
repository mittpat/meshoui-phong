#pragma once

#include <string>
#include <vector>

#include "enums.h"

namespace Meshoui
{
    class Program
    {
    public:
        virtual ~Program();
        Program();
        Program(const std::string & filename);
        void load(const std::string & filename);

        // set before adding
        std::vector<char> vertexShaderSource;
        std::vector<char> fragmentShaderSource;
        Feature::Flags features;
    };
    inline Program::~Program() {}
    inline Program::Program() : features(Feature::Default) {}
    inline Program::Program(const std::string & filename) : features(Feature::Default) { load(filename); }
}
