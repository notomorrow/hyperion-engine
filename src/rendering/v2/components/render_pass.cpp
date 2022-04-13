#include "render_pass.h"

#include "../engine.h"

namespace hyperion::v2 {

RenderPass::RenderPass(RenderPassStage stage, renderer::RenderPass::Mode mode)
    : EngineComponent(stage, mode)
{
}

RenderPass::~RenderPass()
{
    Teardown();
}

void RenderPass::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_RENDER_PASSES, [this](Engine *engine) {
        EngineComponent::Create(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_RENDER_PASSES, [this](Engine *engine) {
            EngineComponent::Destroy(engine);
        }), engine);
    }));
}

} // namespace hyperion::v2