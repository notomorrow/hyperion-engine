#include "texture_3D.h"
#include "../core_engine.h"
#include "../gl_util.h"
#include "../util.h"
#include <cassert>

namespace hyperion {
Texture3D::Texture3D()
    : Texture(TextureType::TEXTURE_TYPE_3D),
      m_length(0)
{
}

Texture3D::Texture3D(int width, int height, int length, unsigned char *bytes)
    : Texture(TextureType::TEXTURE_TYPE_3D, width, height, bytes),
      m_length(length)
{
}

Texture3D::~Texture3D()
{
    // deleted in parent destructor
}

void Texture3D::UploadGpuData(bool should_upload_data)
{
    glTexParameteri(GL_TEXTURE_3D,
        GL_TEXTURE_MAG_FILTER, ToOpenGLFilterMode(mag_filter));
    glTexParameteri(GL_TEXTURE_3D,
        GL_TEXTURE_MIN_FILTER, ToOpenGLFilterMode(min_filter));
    glTexParameteri(GL_TEXTURE_3D,
        GL_TEXTURE_WRAP_S, wrap_s);
    glTexParameteri(GL_TEXTURE_3D,
        GL_TEXTURE_WRAP_T, wrap_t);

    if (should_upload_data) {
        glTexImage3D(GL_TEXTURE_3D, 0, ToOpenGLInternalFormat(ifmt),
            width, height, m_length, 0, ToOpenGLBaseFormat(fmt), ToOpenGLDatumType(GetDatumType()), bytes);

        CatchGLErrors("glTexImage3D failed.", false);

        if (min_filter == Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR_MIPMAP) {
            glGenerateMipmap(GL_TEXTURE_3D);
            CatchGLErrors("Failed to generate Texture3D mipmaps.", false);
        }
    }
}

void Texture3D::CopyData(Texture *const other)
{
    not_implemented;
}

void Texture3D::Use()
{
    glBindTexture(GL_TEXTURE_3D, id);
}

void Texture3D::End()
{
    glBindTexture(GL_TEXTURE_3D, 0);
}

} // namespace hyperion
