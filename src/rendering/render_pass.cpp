#include "render_pass.h"

#include "../engine.h"

namespace hyperion::v2 {

RenderPass::RenderPass(RenderPassStage stage, renderer::RenderPass::Mode mode)
    : EngineComponentBase(),
      m_render_pass(stage, mode)
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

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_RENDER_PASSES, [this](Engine *engine) {
        engine->render_scheduler.Enqueue([this, engine] {
            return m_render_pass.Create(engine->GetDevice());
        });
        //HYPERION_ASSERT_RESULT(m_render_pass.Create(engine->GetDevice()));

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_RENDER_PASSES, [this](Engine *engine) {
            engine->render_scheduler.Enqueue([this, engine] {
                return m_render_pass.Destroy(engine->GetDevice());
            });
            
            engine->render_scheduler.FlushOrWait([](auto &fn) {
                HYPERION_ASSERT_RESULT(fn());
            });

            //HYPERION_ASSERT_RESULT(m_render_pass.Destroy(engine->GetDevice()));
        }), engine);
    }));
}

} // namespace hyperion::v2