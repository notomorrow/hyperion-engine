#include "Texture.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

Texture::Texture(
    Extent3D extent,
    Image::InternalFormat format,
    Image::Type type,
    Image::FilterMode filter_mode,
    Image::WrapMode wrap_mode,
    const UByte *bytes
) : Texture(
        TextureImage(
            extent,
            format,
            type,
            filter_mode,
            bytes
        ),
        filter_mode,
        wrap_mode
    )
{
}

Texture::Texture(
    Image &&image,
    Image::FilterMode filter_mode,
    Image::WrapMode wrap_mode
) : EngineComponentBase(),
    m_image(std::move(image)),
    m_image_view(),
    m_sampler(filter_mode, wrap_mode)
{
}

Texture::~Texture()
{
    Teardown();
}

void Texture::Init(Engine *engine)
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init(engine);

    engine->GetRenderScheduler().Enqueue([this, engine](...) {
        const auto initial_state = renderer::GPUMemory::ResourceState::SHADER_RESOURCE;

        HYPERION_BUBBLE_ERRORS(m_image.Create(engine->GetDevice(), engine->GetInstance(), initial_state));
        HYPERION_BUBBLE_ERRORS(m_image_view.Create(engine->GetInstance()->GetDevice(), &m_image));
        HYPERION_BUBBLE_ERRORS(m_sampler.Create(engine->GetInstance()->GetDevice()));

#if HYP_FEATURES_BINDLESS_TEXTURES
        engine->GetRenderData()->textures.AddResource(this);
#endif
        
        SetReady(true);

        HYPERION_RETURN_OK;
    });
    
    OnTeardown([this]() {
        auto *engine = GetEngine();

        SetReady(false);

        engine->GetRenderScheduler().Enqueue([this, engine](...) {
#if HYP_FEATURES_BINDLESS_TEXTURES
            engine->GetRenderData()->textures.RemoveResource(m_id);
#endif

            HYPERION_BUBBLE_ERRORS(m_sampler.Destroy(engine->GetInstance()->GetDevice()));
            HYPERION_BUBBLE_ERRORS(m_image_view.Destroy(engine->GetInstance()->GetDevice()));
            HYPERION_BUBBLE_ERRORS(m_image.Destroy(engine->GetInstance()->GetDevice()));

            HYPERION_RETURN_OK;
        });
        
        HYP_FLUSH_RENDER_QUEUE(engine);
    });
}

} // namespace hyperion::v2
