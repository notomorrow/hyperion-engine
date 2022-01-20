#include "gi_mapping.h"

#include "../../opengl.h"

namespace apex {
GIMapping::GIMapping(Camera *view_cam, double max_dist)
    : ShadowMapping(view_cam, max_dist),
      m_texture_id(0)
{
}

void GIMapping::Begin()
{
    if (m_texture_id == 0) {
        glGenTextures(1, &m_texture_id);
        glBindTexture(GL_TEXTURE_3D, m_texture_id);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32F, 128, 128, 128);
        glGenerateMipmap(GL_TEXTURE_3D);
        glBindTexture(GL_TEXTURE_3D, 0);
    }

    ShadowMapping::Begin();

    glBindImageTexture(0, m_texture_id, 0, true, 0, GL_READ_WRITE, GL_RGBA32F);

    // TODO clear texture here with compute shader
}

void GIMapping::End()
{
    glBindImageTexture(0, 0, 0, true, 0, GL_READ_WRITE, GL_RGBA32F);

    ShadowMapping::End();
}
} // namespace apex
