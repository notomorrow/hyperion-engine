#include "texture_2D.h"
#include "../core_engine.h"
#include "../gl_util.h"
#include "../util.h"

#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "../util/img/stb_image_resize.h"

#include <cassert>

namespace hyperion {

Texture2D::Texture2D()
    : Texture(TextureType::TEXTURE_TYPE_2D)
{
}

Texture2D::Texture2D(int width, int height, unsigned char *bytes)
    : Texture(TextureType::TEXTURE_TYPE_2D, width, height, bytes)
{
}

Texture2D::~Texture2D()
{
    // deleted in parent destructor
}

void Texture2D::Resize(int new_width, int new_height)
{
    if (bytes == nullptr) {
        width = new_width;
        height = new_height;
        is_uploaded = false;

        return;
    }

    size_t num_components = NumComponents(GetFormat());

    unsigned char *new_data = (unsigned char*)malloc(new_width * new_height * num_components);

    stbir_resize_uint8(
        bytes,
        width,
        height,
        0,
        new_data,
        new_width,
        new_height,
        0,
        num_components
    );

    free(bytes);

    bytes = new_data;
    width = new_width;
    height = new_height;
    is_uploaded = false;
}

void Texture2D::UploadGpuData(bool should_upload_data)
{
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER, ToOpenGLFilterMode(mag_filter));
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER, ToOpenGLFilterMode(min_filter));
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S, wrap_s);
    glTexParameteri(GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T, wrap_t);

    if (should_upload_data) {
        glTexImage2D(GL_TEXTURE_2D, 0, ToOpenGLInternalFormat(ifmt),
            width, height, 0, ToOpenGLBaseFormat(fmt), GL_UNSIGNED_BYTE, bytes);

        CatchGLErrors("glTexImage2D failed.", false);

        if (min_filter == Texture::TextureFilterMode::TEXTURE_FILTER_LINEAR_MIPMAP) {
            glGenerateMipmap(GL_TEXTURE_2D);
            CatchGLErrors("Failed to generate Texture2D mipmaps.", false);
        }
    }
}

void Texture2D::CopyData(Texture * const other)
{
    ex_assert(other != nullptr);
    ex_assert(width == other->GetWidth());
    ex_assert(height == other->GetHeight());
    ex_assert(ifmt == other->GetInternalFormat());
    ex_assert(fmt == other->GetFormat());

    glCopyTexImage2D(GL_TEXTURE_2D, 0, ToOpenGLBaseFormat(fmt), 0, 0, width, height, 0);

    CatchGLErrors("Failed to copy texture data", false);
}

void Texture2D::Use()
{
    glBindTexture(GL_TEXTURE_2D, id);
}

void Texture2D::End()
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace hyperion
