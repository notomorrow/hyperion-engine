#include "framebuffer.h"

namespace hyperion::v2 {

using renderer::AttachmentBase;

Framebuffer::Framebuffer(size_t width, size_t height)
    : BaseComponent(std::make_unique<renderer::FramebufferObject>(width, height))
{
}

Framebuffer::~Framebuffer()
{
}

void Framebuffer::Create(Instance *instance, RenderPass *render_pass)
{
    m_render_pass = non_owning_ptr(render_pass);

    auto result = m_wrapped->Create(instance->GetDevice(), render_pass->GetWrappedObject());
    AssertThrowMsg(result, "%s", result);
}

void Framebuffer::Destroy(Instance *instance)
{
    auto result = m_wrapped->Destroy(instance->GetDevice());
    m_wrapped.reset();
    AssertThrowMsg(result, "%s", result);
}

} // namespace hyperion::v2