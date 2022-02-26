#include "texture.h"
#include "../core_engine.h"
#include "../core_engine.h"
#include "../gl_util.h"
#include "../util.h"

namespace hyperion {

Texture::Texture(TextureType texture_type)
    : m_texture_type(texture_type),
      id(0),
      width(0),
      height(0),
      bytes(nullptr),
      ifmt(TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8),
      fmt(TextureBaseFormat::TEXTURE_FORMAT_RGB),
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
      ifmt(TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8),
      fmt(TextureBaseFormat::TEXTURE_FORMAT_RGB),
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

    // free stb image data
    free(bytes);
}

unsigned int Texture::GetId() const
{
    return id;
}

void Texture::SetFormat(TextureBaseFormat type)
{
    fmt = type;
}

void Texture::SetInternalFormat(TextureInternalFormat type)
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

size_t Texture::NumComponents(TextureBaseFormat format)
{
    switch (format) {
    case TextureBaseFormat::TEXTURE_FORMAT_NONE: return 0;
    case TextureBaseFormat::TEXTURE_FORMAT_R: return 1;
    case TextureBaseFormat::TEXTURE_FORMAT_RG : return 2;
    case TextureBaseFormat::TEXTURE_FORMAT_RGB: return 3;
    case TextureBaseFormat::TEXTURE_FORMAT_RGBA: return 4;
    }

    unexpected_value_msg(format, "Unknown number of components for format");
}

void Texture::ActiveTexture(int i)
{
    glActiveTexture(GL_TEXTURE0 + i);
    CatchGLErrors("Failed to set active texture", true);
}

void Texture::Initialize()
{
    hard_assert(is_created == false && id == 0);

    CoreEngine::GetInstance()->GenTextures(1, &id);

    CatchGLErrors("Failed to generate texture.", false);

    is_created = true;
    is_uploaded = false;
}

void Texture::Deinitialize()
{
    if (is_created) {
        hard_assert(id != 0);

        CoreEngine::GetInstance()->DeleteTextures(1, &id);

        CatchGLErrors("Failed to delete texture.", false);

        is_created = false;
    }
}

void Texture::Prepare(bool should_upload_data)
{
    if (is_created && is_uploaded) {
        return;
    }

    if (!is_created) {
        Initialize();
    }

    Use();

    if (!is_uploaded) {
        UploadGpuData(should_upload_data);

        is_uploaded = true;
    }

    End();
}

int Texture::ToOpenGLInternalFormat(TextureInternalFormat fmt)
{
    switch (fmt) {
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_R8: return GL_R8;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RG8: return GL_RG8;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8: return GL_RGB8;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8: return GL_RGBA8;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_R16: return GL_R16;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RG16: return GL_RG16;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB16: return GL_RGB16;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16: return GL_RGBA16;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_R32: return GL_R32I;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RG32: return GL_RG32I;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB32: return GL_RGB32I;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32: return GL_RGBA32I;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_R16F: return GL_R16F;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RG16F: return GL_RG16F;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB16F: return GL_RGB16F;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA16F: return GL_RGBA16F;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_R32F: return GL_R32F;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RG32F: return GL_RG32F;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB32F: return GL_RGB32F;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F: return GL_RGBA32F;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_16: return GL_DEPTH_COMPONENT16;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_24: return GL_DEPTH_COMPONENT24;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32: return GL_DEPTH_COMPONENT32;
    case TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_DEPTH_32F: return GL_DEPTH_COMPONENT32F;
    }

    unexpected_value_msg(format, "Unhandled texture format case");
}

int Texture::ToOpenGLBaseFormat(TextureBaseFormat fmt)
{
    switch (fmt) {
    case TextureBaseFormat::TEXTURE_FORMAT_R: return GL_RED;
    case TextureBaseFormat::TEXTURE_FORMAT_RG: return GL_RG;
    case TextureBaseFormat::TEXTURE_FORMAT_RGB: return GL_RGB;
    case TextureBaseFormat::TEXTURE_FORMAT_RGBA: return GL_RGBA;
    case TextureBaseFormat::TEXTURE_FORMAT_DEPTH: return GL_DEPTH_COMPONENT;
    }

    unexpected_value_msg(format, "Unhandled texture format case");
}

} // namespace hyperion
