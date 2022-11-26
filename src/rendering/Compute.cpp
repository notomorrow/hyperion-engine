#include "Compute.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {

struct RENDER_COMMAND(CreateComputeShader) : RenderCommand
{
    renderer::ComputePipeline *pipeline;
    renderer::ShaderProgram *shader_program;

    RENDER_COMMAND(CreateComputeShader)(
        renderer::ComputePipeline *pipeline,
        renderer::ShaderProgram *shader_program
    ) : pipeline(pipeline),
        shader_program(shader_program)
    {
    }

    virtual Result operator()()
    {
        return pipeline->Create(
            Engine::Get()->GetGPUDevice(),
            shader_program,
            &Engine::Get()->GetGPUInstance()->GetDescriptorPool()
        );
    }
};

struct RENDER_COMMAND(DestroyComputeShader) : RenderCommand
{
    ComputePipeline &pipeline;

    RENDER_COMMAND(DestroyComputeShader)(ComputePipeline &pipeline)
        : pipeline(pipeline)
    {
    }

    virtual Result operator()()
    {
        return pipeline.GetPipeline()->Destroy(Engine::Get()->GetGPUDevice());
    }
};
    
ComputePipeline::ComputePipeline(Handle<Shader> &&shader)
    : EngineComponentBase(),
      m_shader(std::move(shader))
{
}

ComputePipeline::ComputePipeline(
    Handle<Shader> &&shader,
    const Array<const DescriptorSet *> &used_descriptor_sets
) : EngineComponentBase(),
    m_pipeline(used_descriptor_sets),
    m_shader(std::move(shader))
{
}

ComputePipeline::~ComputePipeline()
{
    Teardown();
}

void ComputePipeline::Init()
{
    if (IsInitCalled()) {
        return;
    }

    EngineComponentBase::Init();

    InitObject(m_shader);

    if (m_shader) {
        RenderCommands::Push<RENDER_COMMAND(CreateComputeShader)>(
            &m_pipeline,
            m_shader->GetShaderProgram()
        );

        HYP_SYNC_RENDER();

        SetReady(true);
    }

    OnTeardown([this]() {
        SetReady(false);

        if (m_shader) {
            RenderCommands::Push<RENDER_COMMAND(DestroyComputeShader)>(*this);
            
            HYP_SYNC_RENDER();
        }
    });
}

} // namespace hyperion::v2
