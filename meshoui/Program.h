#pragma once

#include <string>
#include <vector>

#include "enums.h"

namespace Meshoui
{
    class ProgramPrivate;
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

    private:
        friend class RendererPrivate;
        ProgramPrivate * d;
    };
    inline Program::~Program() {}
    inline Program::Program() : features(Feature::Default), d(nullptr) {}
    inline Program::Program(const std::string & filename) : features(Feature::Default), d(nullptr) { load(filename); }
}
