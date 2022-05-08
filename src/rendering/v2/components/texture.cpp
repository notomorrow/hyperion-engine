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
    Teardown();
}

void Texture::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_TEXTURES, [this](Engine *engine) {
        engine->texture_mutex.lock();
        EngineComponent::Create(
            engine,
            engine->GetInstance(),
            renderer::GPUMemory::ResourceState::SHADER_RESOURCE
        );

        HYPERION_ASSERT_RESULT(m_image_view->Create(engine->GetInstance()->GetDevice(), &m_wrapped));
        HYPERION_ASSERT_RESULT(m_sampler->Create(engine->GetInstance()->GetDevice(), m_image_view.get()));

        engine->shader_globals->textures.AddResource(this);
        engine->texture_mutex.unlock();

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_TEXTURES, [this](Engine *engine) {
            AssertThrow(m_image_view != nullptr);
            AssertThrow(m_sampler != nullptr);
            
            engine->texture_mutex.lock();
            engine->shader_globals->textures.RemoveResource(this);

            HYPERION_ASSERT_RESULT(m_sampler->Destroy(engine->GetInstance()->GetDevice()));
            m_sampler.reset();

            HYPERION_ASSERT_RESULT(m_image_view->Destroy(engine->GetInstance()->GetDevice()));
            m_image_view.reset();

            EngineComponent::Destroy(engine);
            engine->texture_mutex.unlock();
        }), engine);
    }));
}

} // namespace hyperion::v2