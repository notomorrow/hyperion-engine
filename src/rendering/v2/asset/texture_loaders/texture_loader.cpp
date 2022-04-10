#include "texture_loader.h"
#include <rendering/v2/engine.h>
#include <util/img/stb_image.h>

namespace hyperion::v2 {

using TextureLoader = LoaderObject<Texture, LoaderFormat::IMAGE_2D>::Loader;

static const stbi_io_callbacks callbacks{
    .read = [](void *user, char *data, int size) -> int {
        auto *loader_stream = static_cast<LoaderStream *>(user);

        return int(loader_stream->ReadChunked(size_t(size), [&data](unsigned char *buffer, size_t chunk_size) {
            std::memcpy(data, buffer, chunk_size);
            data += chunk_size;
        }));
    },
    .skip = [](void *user, int n) {
        auto *loader_stream = static_cast<LoaderStream *>(user);

        if (n < 0) {
            loader_stream->Rewind(-n);
        } else {
            loader_stream->Skip(n);
        }
    },
    .eof = [](void *user) -> int {
        const auto *loader_stream = static_cast<LoaderStream *>(user);

        return int(loader_stream->Eof());
    }
};

LoaderResult TextureLoader::LoadFn(LoaderStream *stream, Object &object)
{
    unsigned char *image_bytes = stbi_load_from_callbacks(
        &callbacks,
        (void *)stream,
        &object.width,
        &object.height,
        &object.num_components,
        0
    );

    switch (object.num_components) {
    case STBI_rgb_alpha:
        object.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8;
        break;
    case STBI_rgb:
        object.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8;
        break;
    case STBI_grey_alpha:
        object.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RG8;
        break;
    case STBI_grey:
        object.format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_R8;
        break;
    default:
        return {LoaderResult::Status::ERR, "Invalid format -- invalid number of components returned"};
    }

    const size_t image_bytes_count = object.width * object.height * object.num_components;

    object.data.resize(image_bytes_count);
    std::memcpy(&object.data[0], image_bytes, image_bytes_count);

    stbi_image_free(image_bytes);

    return {};
}

std::unique_ptr<Texture> TextureLoader::BuildFn(Engine *engine, const Object &object)
{
    engine->resources.Lock([&](Resources &resources) {
        auto texture = resources.textures.Create(
            std::make_unique<Texture2D>(
                Extent2D{
                    uint32_t(object.width),
                    uint32_t(object.height)
                },
                object.format,
                Image::FilterMode::TEXTURE_FILTER_LINEAR,
                Image::WrapMode::TEXTURE_WRAP_CLAMP_TO_BORDER,
                &object.data[0]
            )
        );
    });

    /* TODO */

    return nullptr;
}

} // namespace hyperion::v2