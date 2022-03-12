#include "framebuffer.h"
#include "../engine.h"

namespace hyperion::v2 {

using renderer::AttachmentBase;

Framebuffer::Framebuffer(size_t width, size_t height)
    : BaseComponent(std::make_unique<renderer::FramebufferObject>(width, height))
{
}

Framebuffer::~Framebuffer()
{
}

void Framebuffer::Create(Engine *engine, RenderPass *render_pass)
{
    auto result = m_wrapped->Create(engine->GetInstance()->GetDevice(), render_pass->GetWrappedObject());
    AssertThrowMsg(result, "%s", result.message);
}

void Framebuffer::Destroy(Engine *engine)
{
    auto result = m_wrapped->Destroy(engine->GetInstance()->GetDevice());
    m_wrapped.reset();
    AssertThrowMsg(result, "%s", result.message);
}

} // namespace hyperion::v2