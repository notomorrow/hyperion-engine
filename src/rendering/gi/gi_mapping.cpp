#include "gi_mapping.h"

#include "../../opengl.h"
#include "../../gl_util.h"

namespace hyperion {
GIMapping::GIMapping()
    : m_texture_id(0)
{
}

GIMapping::~GIMapping()
{
    if (m_texture_id != 0) {
        glDeleteTextures(1, &m_texture_id);
    }
}

void GIMapping::Begin()
{
    if (m_texture_id == 0) {
        glGenTextures(1, &m_texture_id);
        CatchGLErrors("Failed to generate textures.");
        glBindTexture(GL_TEXTURE_3D, m_texture_id);
        CatchGLErrors("Failed to bind 3d texture.");
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        CatchGLErrors("Failed to init 3d texture parameters.");
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32F, 128, 128, 128);
        CatchGLErrors("Failed to set 3d texture storage.");
        glGenerateMipmap(GL_TEXTURE_3D);
        //CatchGLErrors("Failed to gen 3d texture mipmaps.");
        glBindTexture(GL_TEXTURE_3D, 0);
    }

    glBindImageTexture(0, m_texture_id, 0, true, 0, GL_READ_WRITE, GL_RGBA32F);
    CatchGLErrors("Failed to bind image texture.");
}

void GIMapping::End()
{
    glBindImageTexture(0, 0, 0, true, 0, GL_READ_WRITE, GL_RGBA32F);
}
} // namespace apex