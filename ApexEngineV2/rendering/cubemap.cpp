#include "./cubemap.h"
#include "../util.h"

#include <iostream>

namespace apex {

Cubemap::Cubemap(const std::array<std::shared_ptr<Texture2D>, 6> &textures)
    : Texture(),
      m_textures(textures)
{
    is_uploaded = false;
    is_created = false;
}

Cubemap::~Cubemap()
{
    if (is_created) {
        glDeleteTextures(1, &id);
    }

    is_uploaded = false;
    is_created = false;
}

void Cubemap::Use()
{
    if (!is_created) {
        glGenTextures(1, &id);
        CatchGLErrors("Failed to generate cubemap texture", false);

        glEnable(GL_TEXTURE_CUBE_MAP);
        CatchGLErrors("Failed to enable GL_TEXTURE_CUBE_MAP", false);
        //glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

        is_created = true;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, id);

    if (!is_uploaded) {
        for (size_t i = 0; i < m_textures.size(); i++) {
            const auto &tex = m_textures[i];

            if (tex == nullptr) {
                throw std::runtime_error("Could not upload cubemap because texture #" + std::to_string(i + 1) + " was nullptr.");
            } else if (tex->GetBytes() == nullptr) {
                throw std::runtime_error("Could not upload cubemap because texture #" + std::to_string(i + 1) + " had no bytes set.");
            }

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, tex->GetInternalFormat(),
                tex->GetWidth(), tex->GetHeight(), 0, tex->GetFormat(), GL_UNSIGNED_BYTE, tex->GetBytes());
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

        CatchGLErrors("Failed to upload cubemap");

        is_uploaded = true;
    }

}

void Cubemap::End()
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

} // namespace apex
