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
    : EngineComponentBase(),
      m_image(std::make_unique<TextureImage>(extent, format, type, filter_mode, bytes)),
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

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_TEXTURES, [this](Engine *engine) {
        auto queued_id = engine->render_scheduler.Enqueue([this, engine] {
            HYPERION_ASSERT_RESULT(m_image->Create(engine->GetDevice(), engine->GetInstance(), renderer::GPUMemory::ResourceState::SHADER_RESOURCE));
            HYPERION_ASSERT_RESULT(m_image_view->Create(engine->GetInstance()->GetDevice(), m_image.get()));
            HYPERION_ASSERT_RESULT(m_sampler->Create(engine->GetInstance()->GetDevice(), m_image_view.get()));

            engine->shader_globals->textures.AddResource(engine->resources.textures.Acquire(this));

            HYPERION_RETURN_OK;
        });
        
        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_TEXTURES, [this, queued_id](Engine *engine) {
            engine->render_scheduler.DequeueOrEnqueue(queued_id, [this, engine] {
                AssertThrow(m_image != nullptr);
                AssertThrow(m_image_view != nullptr);
                AssertThrow(m_sampler != nullptr);
                
                engine->shader_globals->textures.RemoveResource(m_id);

                HYPERION_ASSERT_RESULT(m_sampler->Destroy(engine->GetInstance()->GetDevice()));

                HYPERION_ASSERT_RESULT(m_image_view->Destroy(engine->GetInstance()->GetDevice()));

                HYPERION_ASSERT_RESULT(m_image->Destroy(engine->GetInstance()->GetDevice()));

                HYPERION_RETURN_OK;
            });

            engine->render_scheduler.WaitForFlush();
            
            m_sampler.reset();
            m_image_view.reset();
            m_image.reset();
        }), engine);
    }));
}

} // namespace hyperion::v2