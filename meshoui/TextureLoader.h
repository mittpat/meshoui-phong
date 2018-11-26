#pragma once

#include <string>

namespace Meshoui
{
    namespace TextureLoader
    {
        bool loadPNG(/*GLuint * buffer, */const std::string & filename, bool repeat = false);
        bool loadDDS(/*GLuint * buffer, */const std::string & filename, bool repeat = false);
    }
}
