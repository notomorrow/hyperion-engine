#include "texture_loader.h"

#include "../util/img/stb_image.h"

#include "../math/math_util.h"
#include "../rendering/texture_2D.h"
#include "../opengl.h"

#include <iostream>

namespace hyperion {
AssetLoader::Result TextureLoader::LoadFromFile(const std::string &path)
{
    int width, height, comp;
    unsigned char *bytes = stbi_load(path.c_str(), &width, &height, &comp, 0);

    if (bytes == nullptr) {
        return AssetLoader::Result(AssetLoader::Result::ASSET_ERR, "Texture data could not be read.");
    }

    auto tex = std::make_shared<Texture2D>(width, height, bytes);

    switch (comp) {
        case STBI_rgb_alpha:
            tex->SetFormat(Texture::TextureBaseFormat::TEXTURE_FORMAT_RGBA);
            tex->SetInternalFormat(Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8);
            break;
        case STBI_rgb:
            tex->SetFormat(Texture::TextureBaseFormat::TEXTURE_FORMAT_RGB);
            tex->SetInternalFormat(Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8);
            break;
        case STBI_grey_alpha:
            tex->SetFormat(Texture::TextureBaseFormat::TEXTURE_FORMAT_RG);
            tex->SetInternalFormat(Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RG8);
            break;
        case STBI_grey:
            tex->SetFormat(Texture::TextureBaseFormat::TEXTURE_FORMAT_R);
            tex->SetInternalFormat(Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_R8);
            break;
        default:
            std::cout << "Unknown image format!" << std::endl;
            throw "Unknown image format";
    }

    bool resize = false;
    size_t new_width = width, new_height = height;

    if (!MathUtil::IsPowerOfTwo(width)) {
        resize = true;
        new_width = MathUtil::NextPowerOf2(width);
    }

    if (!MathUtil::IsPowerOfTwo(height)) {
        resize = true;
        new_height = MathUtil::NextPowerOf2(height);
    }

    if (resize) {
        tex->Resize(new_width, new_height);
    }

    return AssetLoader::Result(tex);
}
} // namespace hyperion
