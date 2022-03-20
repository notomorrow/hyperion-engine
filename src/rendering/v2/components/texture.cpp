#include "texture.h"
#include "../engine.h"

namespace hyperion::v2 {

Texture::Texture(size_t width, size_t height, size_t depth,
    Image::InternalFormat format,
    Image::Type type,
    Image::FilterMode filter_mode,
    Image::WrapMode wrap_mode,
    unsigned char *bytes)
    : EngineComponent(width, height, depth, format, type, filter_mode, bytes),
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
    EngineComponent::Create(engine);

    auto image_view_result = m_image_view->Create(engine->GetInstance()->GetDevice(), &m_wrapped);
    AssertThrowMsg(image_view_result, "%s", image_view_result.message);

    auto sampler_result = m_sampler->Create(engine->GetInstance()->GetDevice(), m_image_view.get());
    AssertThrowMsg(sampler_result, "%s", sampler_result.message);
}

void Texture::Destroy(Engine *engine)
{
    auto sampler_result = m_sampler->Destroy(engine->GetInstance()->GetDevice());
    AssertThrowMsg(sampler_result, "%s", sampler_result.message);

    auto image_view_result = m_image_view->Destroy(engine->GetInstance()->GetDevice());
    AssertThrowMsg(image_view_result, "%s", image_view_result.message);

    EngineComponent::Destroy(engine);
}

} // namespace hyperion::v2