#include "Compute.hpp"
#include "../Engine.hpp"

namespace hyperion::v2 {
    
ComputePipeline::ComputePipeline(Handle<Shader> shader)
    : BasicObject(),
      m_shader(std::move(shader)),
      m_pipeline(MakeRenderObject<renderer::ComputePipeline>(m_shader->GetShaderProgram()))
{
}

ComputePipeline::ComputePipeline(
    Handle<Shader> shader,
    const Array<DescriptorSetRef> &used_descriptor_sets
) : BasicObject(),
    m_shader(std::move(shader)),
    m_pipeline(MakeRenderObject<renderer::ComputePipeline>(m_shader->GetShaderProgram(), used_descriptor_sets))
{
}

ComputePipeline::~ComputePipeline()
{
    SetReady(false);

    SafeRelease(std::move(m_pipeline));
    m_shader.Reset();
}

void ComputePipeline::Init()
{
    if (IsInitCalled()) {
        return;
    }

    BasicObject::Init();
    
    if (InitObject(m_shader)) {
        DeferCreate(
            m_pipeline,
            g_engine->GetGPUDevice(),
            &g_engine->GetGPUInstance()->GetDescriptorPool()
        );

        SetReady(true);
    }
}

} // namespace hyperion::v2
