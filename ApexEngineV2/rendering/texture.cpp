#include "texture.h"
#include "../gl_util.h"
#include "../core_engine.h"

namespace apex {

Texture::Texture(TextureType texture_type)
    : m_texture_type(texture_type),
      id(0),
      width(0),
      height(0),
      bytes(nullptr),
      ifmt(GL_RGB8),
      fmt(GL_RGB),
      mag_filter(GL_LINEAR),
      min_filter(GL_LINEAR_MIPMAP_LINEAR),
      wrap_s(GL_REPEAT),
      wrap_t(GL_REPEAT),
      is_uploaded(false),
      is_created(false)
{
}

Texture::Texture(TextureType texture_type, int width, int height, unsigned char *bytes)
    : m_texture_type(texture_type),
      id(0),
      width(width),
      height(height),
      bytes(bytes),
      ifmt(GL_RGB8),
      fmt(GL_RGB),
      mag_filter(GL_LINEAR),
      min_filter(GL_LINEAR_MIPMAP_LINEAR),
      wrap_s(GL_REPEAT),
      wrap_t(GL_REPEAT),
      is_uploaded(false),
      is_created(false)
{
}

Texture::~Texture()
{
    Deinitialize();

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

void Texture::Initialize()
{
    assert(is_created == false && id == 0);

    CoreEngine::GetInstance()->GenTextures(1, &id);

    CatchGLErrors("Failed to generate texture.", false);

    is_created = true;
    is_uploaded = false;
}

void Texture::Deinitialize()
{
    if (is_created) {
        assert(id != 0);

        CoreEngine::GetInstance()->DeleteTextures(1, &id);

        CatchGLErrors("Failed to delete texture.", false);

        is_created = false;
    }
}

void Texture::Prepare()
{
    if (is_created && is_uploaded) {
        return;
    }

    if (!is_created) {
        Initialize();
    }

    Use();

    if (!is_uploaded) {
        UploadGpuData();

        is_uploaded = true;
    }

    End();
}

} // namespace apex
