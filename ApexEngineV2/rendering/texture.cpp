#include "texture.h"
#include "../core_engine.h"

namespace apex {
Texture::Texture()
    : width(0), height(0), bytes(NULL), 
      ifmt(CoreEngine::RGB), fmt(CoreEngine::RGB),
      mag_filter(CoreEngine::LINEAR), min_filter(CoreEngine::LINEAR_MIPMAP_LINEAR),
      wrap_s(CoreEngine::REPEAT), wrap_t(CoreEngine::REPEAT)
{
}

Texture::Texture(int width, int height, unsigned char *bytes)
    : width(width), height(height), bytes(bytes), 
      ifmt(CoreEngine::RGB), fmt(CoreEngine::RGB),
      mag_filter(CoreEngine::LINEAR), min_filter(CoreEngine::LINEAR_MIPMAP_LINEAR),
      wrap_s(CoreEngine::REPEAT), wrap_t(CoreEngine::REPEAT)
{
}

Texture::~Texture()
{
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
    CoreEngine::GetInstance()->ActiveTexture(CoreEngine::TEXTURE0 + i);
}
}