#include "render_pass.h"

namespace hyperion::v2 {

using renderer::Attachment;

RenderPass::RenderPass()
    : BaseComponent(std::make_unique<renderer::RenderPass>())
{
}

RenderPass::~RenderPass() = default;

void RenderPass::Create(Instance *instance)
{
    auto result = m_wrapped->Create(instance->GetDevice());
    AssertThrowMsg(result, "%s", result);
}

void RenderPass::Destroy(Instance *instance)
{
    auto result = m_wrapped->Destroy(instance->GetDevice());
    m_wrapped.reset();

    AssertThrowMsg(result, "%s", result);
}

} // namespace hyperion::v2