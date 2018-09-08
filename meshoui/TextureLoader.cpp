#include <GL/glew.h>
#include "TextureLoader.h"

#include <SDL2/SDL_image.h>
#include <nv_dds.h>

#include <experimental/filesystem>
#include <fstream>
#include <vector>

using namespace nv_dds;
namespace std { namespace filesystem = experimental::filesystem; }

bool TextureLoader::loadPNG(GLuint * buffer, const std::string & filename)
{
    if (std::filesystem::path(filename).extension() != ".png")
        return false;

    if (!std::filesystem::exists(filename))
        return false;

    glGenTextures(1, buffer);

    SDL_Surface * image = IMG_Load(filename.c_str());

    glBindTexture(GL_TEXTURE_2D, *buffer);

    GLenum mode = image->format->BytesPerPixel == 4 ? GL_RGBA : GL_RGB;

    glTexImage2D(GL_TEXTURE_2D, 0, mode, image->w, image->h, 0, mode, GL_UNSIGNED_BYTE, image->pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);

    SDL_FreeSurface(image);

    return true;
}

bool TextureLoader::loadDDS(GLuint * buffer, const std::string & filename)
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
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
