#include "texture.h"
#include "../gl_util.h"

namespace apex {

Texture::Texture()
    : width(0),
      height(0),
      bytes(nullptr),
      ifmt(GL_RGB8),
      fmt(GL_RGB),
      mag_filter(GL_LINEAR),
      min_filter(GL_LINEAR_MIPMAP_LINEAR),
      wrap_s(GL_REPEAT),
      wrap_t(GL_REPEAT)
{
}

Texture::Texture(int width, int height, unsigned char *bytes)
    : width(width),
      height(height),
      bytes(bytes),
      ifmt(GL_RGB8),
      fmt(GL_RGB),
      mag_filter(GL_LINEAR),
      min_filter(GL_LINEAR_MIPMAP_LINEAR),
      wrap_s(GL_REPEAT),
      wrap_t(GL_REPEAT)
{
}

Texture::~Texture()
{
    free(bytes);
}

unsigned int Texture::GetId() const
{
    return id;
}

void Texture::SetFormat(int type)
{
    fmt = type;
}

void Texture::SetInternalFormat(int type)
{
    ifmt = type;
}

void Texture::SetFilter(int mag, int min)
{
    mag_filter = mag;
    min_filter = min;
}

void Texture::SetWrapMode(int s, int t)
{
    wrap_s = s;
    wrap_t = t;
}

void Texture::ActiveTexture(int i)
{
    glActiveTexture(GL_TEXTURE0 + i);
    CatchGLErrors("Failed to set active texture", false);
}

} // namespace apex
