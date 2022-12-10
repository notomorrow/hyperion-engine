#include "Texture.hpp"
#include <Engine.hpp>
#include <rendering/backend/RendererFeatures.hpp>

namespace hyperion::v2 {

using renderer::Result;

class Texture;

#pragma region Render commands

struct RENDER_COMMAND(CreateTexture) : RenderCommand
{
    Texture *texture;
    renderer::ResourceState initial_state;
    renderer::Image *image;
    renderer::ImageView *image_view;
    renderer::Sampler *sampler;

    RENDER_COMMAND(CreateTexture)(
        Texture *texture,
        renderer::ResourceState initial_state,
        renderer::Image *image,
        renderer::ImageView *image_view,
        renderer::Sampler *sampler
    ) : texture(texture),
        initial_state(initial_state),
        image(image),
        image_view(image_view),
        sampler(sampler)
    {
    }

    virtual Result operator()()
    {
        HYPERION_BUBBLE_ERRORS(image->Create(Engine::Get()->GetGPUDevice(), Engine::Get()->GetGPUInstance(), initial_state));
        HYPERION_BUBBLE_ERRORS(image_view->Create(Engine::Get()->GetGPUInstance()->GetDevice(), image));
        HYPERION_BUBBLE_ERRORS(sampler->Create(Engine::Get()->GetGPUInstance()->GetDevice()));
        
        if (Engine::Get()->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            Engine::Get()->GetRenderData()->textures.AddResource(texture);
        }

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(DestroyTexture) : RenderCommand
{
    ID<Texture> id;
    renderer::Image *image;
    renderer::ImageView *image_view;
    renderer::Sampler *sampler;

    RENDER_COMMAND(DestroyTexture)(
        ID<Texture> id,
        renderer::Image *image,
        renderer::ImageView *image_view,
        renderer::Sampler *sampler
    ) : id(id),
        image(image),
        image_view(image_view),
        sampler(sampler)
    {
    }

    virtual Result operator()()
    {
        if (Engine::Get()->GetGPUDevice()->GetFeatures().SupportsBindlessTextures()) {
            Engine::Get()->GetRenderData()->textures.RemoveResource(id);
        }

        HYPERION_BUBBLE_ERRORS(sampler->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()));
        HYPERION_BUBBLE_ERRORS(image_view->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()));
        HYPERION_BUBBLE_ERRORS(image->Destroy(Engine::Get()->GetGPUInstance()->GetDevice()));

        HYPERION_RETURN_OK;
    }
};

#pragma endregion

Texture::Texture(
    Extent3D extent,
    InternalFormat format,
    ImageType type,
    FilterMode filter_mode,
    WrapMode wrap_mode,
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
    FilterMode filter_mode,
    WrapMode wrap_mode
) : EngineComponentBase(),
    m_image(std::move(image)),
    m_image_view(),
    m_sampler(filter_mode, wrap_mode)
{
}

Texture::Texture(Texture &&other) noexcept
    : EngineComponentBase(std::move(other)),
      m_image(std::move(other.m_image)),
      m_image_view(std::move(other.m_image_view)),
      m_sampler(std::move(other.m_sampler))
{
}

// Texture &Texture::operator=(Texture &&other) noexcept
// {
//     EngineComponentBase::operator=(std::move(other));

//     m_image = std::move(other.m_image),
//     m_image_view = std::move(other.m_image_view),
//     m_sampler = std::move(other.m_sampler);

//     return *this;
// }

Texture::~Texture()
{
    SetReady(false);

    if (IsInitCalled()) {
        RenderCommands::Push<RENDER_COMMAND(DestroyTexture)>(
            m_id,
            &m_image,
            &m_image_view,
            &m_sampler
        );
        
        HYP_SYNC_RENDER();
    }
}

void Texture::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    RenderCommands::Push<RENDER_COMMAND(CreateTexture)>(
        this,
        renderer::ResourceState::SHADER_RESOURCE,
        &m_image,
        &m_image_view,
        &m_sampler
    );

    SetReady(true);
}

} // namespace hyperion::v2
