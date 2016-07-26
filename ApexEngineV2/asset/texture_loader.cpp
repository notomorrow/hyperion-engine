#include "texture_loader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../util/img/stb_image.h"

#include "../rendering/texture_2D.h"
#include "../core_engine.h"

namespace apex {
std::shared_ptr<Loadable> TextureLoader::LoadFromFile(const std::string &path)
{
    int w, h, comp;
    unsigned char *bytes = stbi_load(path.c_str(), &w, &h, &comp, STBI_rgb_alpha);

    if (bytes == nullptr) {
        return nullptr;
    }

    auto tex = std::make_shared<Texture2D>();
    tex->width = w;
    tex->height = h;
    tex->bytes = bytes;

    if (comp == 4) {
        tex->SetFormat(CoreEngine::RGBA);
    } else {
        tex->SetFormat(CoreEngine::RGB);
    }

    tex->Use(); // upload data

    stbi_image_free(bytes);
    tex->bytes = nullptr;

    tex->GenerateMipMap();
    tex->End();

    return tex;
}
}