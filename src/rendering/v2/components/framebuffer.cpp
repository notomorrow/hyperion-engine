#include "framebuffer.h"
#include "../engine.h"

namespace hyperion::v2 {

Framebuffer::Framebuffer(Extent2D extent, Ref<RenderPass> &&render_pass)
    : EngineComponent(extent),
      m_render_pass(std::move(render_pass))
{
}

Framebuffer::~Framebuffer()
{
    Teardown();
}

/*void Framebuffer::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }
    
    OnInit(engine->callbacks.Once(EngineCallback::CREATE_FRAMEBUFFERS, [this](Engine *engine) {
        AssertThrowMsg(m_render_pass != nullptr, "Render pass must be set on framebuffer.");
        
        m_render_pass.Init();

        EngineComponent::Create(engine, &m_render_pass->Get());

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_FRAMEBUFFERS, [this](Engine *engine) {
            EngineComponent::Destroy(engine);
        }), engine);
    }));
}*/

void Framebuffer::Create(Engine *engine)
{
    m_render_pass.Init();

    EngineComponent::Create(engine, &m_render_pass->Get());
}

void Framebuffer::Destroy(Engine *engine)
{
    EngineComponent::Destroy(engine);
}

} // namespace hyperion::v2