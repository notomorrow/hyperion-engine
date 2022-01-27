#include "./cubemap.h"
#include "../gl_util.h"
#include "../util.h"
#include "../core_engine.h"
#include "../math/math_util.h"

#include <iostream>

namespace hyperion {

Cubemap::Cubemap(const std::array<std::shared_ptr<Texture2D>, 6> &textures)
    : Texture(TextureType::TEXTURE_TYPE_3D),
      m_textures(textures)
{
}

Cubemap::~Cubemap()
{
}

void Cubemap::Initialize()
{
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::TEXTURE_CUBE_MAP);

    CatchGLErrors("Failed to enable GL_TEXTURE_CUBE_MAP", false);

    Texture::Initialize();
}

void Cubemap::CopyData(Texture * const other)
{
    not_implemented;
}

void Cubemap::UploadGpuData(bool should_upload_data)
{
    if (should_upload_data) {
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
    }

    CoreEngine::GetInstance()->TexParameteri(
        CoreEngine::GLEnums::TEXTURE_CUBE_MAP,
        CoreEngine::GLEnums::TEXTURE_MAG_FILTER,
        CoreEngine::GLEnums::LINEAR
    );
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    // glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, CUBEMAP_NUM_MIPMAPS);

    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    CatchGLErrors("Failed to upload cubemap");
}

void Cubemap::Use()
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, id);
}

void Cubemap::End()
{
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

} // namespace hyperion
