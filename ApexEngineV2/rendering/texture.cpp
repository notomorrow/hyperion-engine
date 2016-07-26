#include "texture.h"
#include "../core_engine.h"

namespace apex {
Texture::Texture()
    : width(0), height(0), bytes(NULL), 
      ifmt(CoreEngine::RGB), fmt(CoreEngine::RGB)
{
}

Texture::Texture(int width, int height, unsigned char *bytes)
    : width(width), height(height), bytes(bytes), 
      ifmt(CoreEngine::RGB), fmt(CoreEngine::RGB)
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
}