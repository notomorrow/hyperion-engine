#include "texture.h"
#include "../engine.h"

namespace hyperion::v2 {

Texture::Texture(Extent3D extent,
    Image::InternalFormat format,
    Image::Type type,
    Image::FilterMode filter_mode,
    Image::WrapMode wrap_mode,
    unsigned char *bytes)
    : EngineComponent(extent, format, type, filter_mode, bytes),
      m_image_view(std::make_unique<ImageView>()),
      m_sampler(std::make_unique<Sampler>(filter_mode, wrap_mode))
{
}

Texture::~Texture()
{
    AssertThrowMsg(m_image_view == nullptr, "image view should have been destroyed");
    AssertThrowMsg(m_sampler == nullptr, "sampler should have been destroyed");
}

void Texture::Create(Engine *engine)
{
    EngineComponent::Create(
        engine,
        engine->GetInstance(),
        Image::LayoutTransferState<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL>{},
        Image::LayoutTransferState<VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>{}
    );

    HYPERION_ASSERT_RESULT(m_image_view->Create(engine->GetInstance()->GetDevice(), &m_wrapped));
    HYPERION_ASSERT_RESULT(m_sampler->Create(engine->GetInstance()->GetDevice(), m_image_view.get()));
}

void Texture::Destroy(Engine *engine)
{
    HYPERION_ASSERT_RESULT(m_sampler->Destroy(engine->GetInstance()->GetDevice()));
    m_sampler.reset();

    HYPERION_ASSERT_RESULT(m_image_view->Destroy(engine->GetInstance()->GetDevice()));
    m_image_view.reset();

    EngineComponent::Destroy(engine);
}


TextureArray::TextureArray(Extent3D extent,
    Image::InternalFormat format,
    Image::Type type,
    Image::FilterMode filter_mode,
    Image::WrapMode wrap_mode
) : m_extent(extent),
    m_format(format),
    m_type(type),
    m_filter_mode(filter_mode),
    m_wrap_mode(wrap_mode)
{
}

TextureArray::TextureArray(const TextureArray &other)
    : m_extent(other.m_extent),
      m_format(other.m_format),
      m_type(other.m_type),
      m_filter_mode(other.m_filter_mode),
      m_wrap_mode(other.m_wrap_mode),
      m_images(other.m_images),
      m_image_views(other.m_image_views),
      m_samplers(other.m_samplers),
      m_index_map(other.m_index_map)
{
}
TextureArray &TextureArray::operator=(const TextureArray &other)
{
    m_extent = other.m_extent;
    m_format = other.m_format;
    m_type = other.m_type;
    m_filter_mode = other.m_filter_mode;
    m_wrap_mode = other.m_wrap_mode;
    m_images = other.m_images;
    m_image_views = other.m_image_views;
    m_samplers = other.m_samplers;
    m_index_map = other.m_index_map;

    return *this;
}

TextureArray::~TextureArray() = default;

void TextureArray::AddTexture(Engine *engine, Texture::ID texture_id)
{
    Texture *texture = engine->resources.textures[texture_id];
    AssertThrow(texture != nullptr);

    AssertThrowMsg(texture->Get().GetExtent() == m_extent,                  "sizes must match");
    AssertThrowMsg(texture->Get().GetTextureFormat() == m_format,           "formats must match");
    AssertThrowMsg(texture->Get().GetType() == m_type,                      "types must match");
    AssertThrowMsg(texture->GetSampler()->GetFilterMode() == m_filter_mode, "filter modes must match");
    AssertThrowMsg(texture->GetSampler()->GetWrapMode() == m_wrap_mode,     "wrap modes must match");

    m_images.push_back(&texture->Get());
    m_image_views.push_back(texture->GetImageView());
    m_samplers.push_back(texture->GetSampler());
    m_index_map[texture_id] = m_images.size() - 1;
}

void TextureArray::RemoveTexture(Texture::ID texture_id)
{
    auto it = m_index_map.find(texture_id);

    if (it == m_index_map.end()) {
        return;
    }

    const size_t index = it->second;

    m_images.erase(m_images.begin() + index);
    m_image_views.erase(m_image_views.begin() + index);
    m_samplers.erase(m_samplers.begin() + index);
}

} // namespace hyperion::v2