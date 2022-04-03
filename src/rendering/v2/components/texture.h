#ifndef HYPERION_V2_TEXTURE_H
#define HYPERION_V2_TEXTURE_H

#include "base.h"

#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_image_view.h>
#include <rendering/backend/renderer_sampler.h>

#include <memory>
#include <map>

namespace hyperion::v2 {

using renderer::Image;
using renderer::TextureImage;
using renderer::ImageView;
using renderer::Sampler;

class Texture : public EngineComponent<TextureImage> {
public:
    Texture(size_t width, size_t height, size_t depth,
        Image::InternalFormat format,
        Image::Type type,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode,
        unsigned char *bytes);

    Texture(const Texture &other) = delete;
    Texture &operator=(const Texture &other) = delete;
    ~Texture();

    inline ImageView *GetImageView() const { return m_image_view.get(); }
    inline Sampler *GetSampler() const { return m_sampler.get(); }

    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    std::unique_ptr<ImageView> m_image_view;
    std::unique_ptr<Sampler> m_sampler;
};

class Texture2D : public Texture {
public:
    Texture2D(size_t width, size_t height,
        Image::InternalFormat format,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode,
        unsigned char *bytes
    ) : Texture(
        width, height, 1,
        format,
        Image::Type::TEXTURE_TYPE_2D,
        filter_mode,
        wrap_mode,
        bytes
    ) {}
};

class Texture3D : public Texture {
public:
    Texture3D(size_t width, size_t height, size_t depth,
        Image::InternalFormat format,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode,
        unsigned char *bytes
    ) : Texture(
        width, height, depth,
        format,
        Image::Type::TEXTURE_TYPE_3D,
        filter_mode,
        wrap_mode,
        bytes
    ) {}
};

class TextureCube : public Texture {
public:
    TextureCube(size_t face_width, size_t face_height,
        Image::InternalFormat format,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode,
        unsigned char *bytes
    ) : Texture(face_width, face_height, 1,
        format,
        Image::Type::TEXTURE_TYPE_CUBEMAP,
        filter_mode,
        wrap_mode,
        bytes
    ) {}
};

class TextureArray {
public:
    TextureArray(size_t width, size_t height, size_t depth,
        Image::InternalFormat format,
        Image::Type type,
        Image::FilterMode filter_mode,
        Image::WrapMode wrap_mode);
    TextureArray(const TextureArray &other);
    TextureArray &operator=(const TextureArray &other);
    ~TextureArray();

    void AddTexture(Engine *engine, Texture::ID texture_id);
    void RemoveTexture(Texture::ID texture_id);

private:
    size_t m_width;
    size_t m_height;
    size_t m_depth;
    Image::InternalFormat m_format;
    Image::Type m_type;
    Image::FilterMode m_filter_mode;
    Image::WrapMode m_wrap_mode;

    std::vector<Image *> m_images;
    std::vector<ImageView *> m_image_views;
    std::vector<Sampler *> m_samplers;
    std::map<Texture::ID, size_t> m_index_map;
};

} // namespace hyperion::v2

#endif