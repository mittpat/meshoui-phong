#include <GL/glew.h>
#include "TextureLoader.h"

#include <SDL2/SDL_image.h>
#include <lodepng.h>
#include <nv_dds.h>

#include <experimental/filesystem>
#include <fstream>
#include <vector>

using namespace nv_dds;
namespace std { namespace filesystem = experimental::filesystem; }

bool TextureLoader::loadPNG(GLuint * buffer, const std::string & filename, bool repeat)
{
    if (std::filesystem::path(filename).extension() != ".png")
        return false;

    if (!std::filesystem::exists(filename))
        return false;

    glGenTextures(1, buffer);

    std::vector<unsigned char> image;
    unsigned width, height;
    unsigned error = lodepng::decode(image, width, height, filename);

    if (error != 0)
    {
        printf("TextureLoader::loadPNG: error '%d' : '%s'\n", error, lodepng_error_text(error));
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, *buffer);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GLsizei(width), GLsizei(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_BORDER);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}

bool TextureLoader::loadDDS(GLuint * buffer, const std::string & filename, bool repeat)
{
    if (std::filesystem::path(filename).extension() != ".dds")
        return false;

    if (!std::filesystem::exists(filename))
        return false;

    CDDSImage image;
    image.load(filename, false);

    glGenTextures(1, buffer);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, *buffer);

    glCompressedTexImage2DARB(GL_TEXTURE_2D, 0, image.get_format(), image.get_width(), image.get_height(), 0, image.get_size(), image);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_BORDER);
    for (unsigned int i = 0; i < image.get_num_mipmaps(); i++)
    {
        CSurface mipmap = image.get_mipmap(i);
        glCompressedTexImage2DARB(GL_TEXTURE_2D, i+1, image.get_format(), mipmap.get_width(), mipmap.get_height(), 0, mipmap.get_size(), mipmap);
    }
    if (image.get_num_mipmaps() == 0)
    {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    return true;
}
