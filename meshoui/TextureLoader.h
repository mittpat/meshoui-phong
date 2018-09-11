#pragma once

#include "gltypes.h"

#include <string>

namespace TextureLoader
{
    bool loadPNG(GLuint * buffer, const std::string & filename, bool repeat = false);
    bool loadDDS(GLuint * buffer, const std::string & filename, bool repeat = false);
}
