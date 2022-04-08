#include "texture.h"
#include "../engine.h"

namespace hyperion::v2 {

Texture::Texture(
    Extent3D extent,
    Image::InternalFormat format,
    Image::Type type,
    Image::FilterMode filter_mode,
    Image::WrapMode wrap_mode,
    const unsigned char *bytes)
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

} // namespace hyperion::v2