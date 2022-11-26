#include "RenderPass.hpp"

#include "../Engine.hpp"

namespace hyperion::v2 {

using renderer::Result;

struct RENDER_COMMAND(CreateRenderPass) : RenderCommand
{
    renderer::RenderPass *render_pass;

    RENDER_COMMAND(CreateRenderPass)(renderer::RenderPass *render_pass)
        : render_pass(render_pass)
    {
    }

    virtual Result operator()()
    {
        return render_pass->Create(Engine::Get()->GetGPUDevice());
    }
};

struct RENDER_COMMAND(DestroyRenderPass) : RenderCommand
{
    renderer::RenderPass *render_pass;

    RENDER_COMMAND(DestroyRenderPass)(renderer::RenderPass *render_pass)
        : render_pass(render_pass)
    {
    }

    virtual Result operator()()
    {
        return render_pass->Destroy(Engine::Get()->GetGPUDevice());
    }
};

RenderPass::RenderPass(RenderPassStage stage, renderer::RenderPass::Mode mode)
    : EngineComponentBase(),
      m_render_pass(stage, mode)
{
}

RenderPass::RenderPass(RenderPassStage stage, renderer::RenderPass::Mode mode, UInt num_multiview_layers)
    : EngineComponentBase(),
      m_render_pass(stage, mode, num_multiview_layers)
{
}

RenderPass::~RenderPass()
{
    Teardown();
}

void RenderPass::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    RenderCommands::Push<RENDER_COMMAND(CreateRenderPass)>(&m_render_pass);

    SetReady(true);

    OnTeardown([this]() {
        RenderCommands::Push<RENDER_COMMAND(DestroyRenderPass)>(&m_render_pass);

        SetReady(false);
        
        HYP_SYNC_RENDER();
    });
}

} // namespace hyperion::v2
