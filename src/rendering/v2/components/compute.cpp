#include "compute.h"
#include "../engine.h"

namespace hyperion::v2 {
ComputePipeline::ComputePipeline(Ref<Shader> &&shader)
    : EngineComponent(),
      m_shader(std::move(shader))
{
}

ComputePipeline::~ComputePipeline() = default;

void ComputePipeline::Create(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    AssertThrow(m_shader != nullptr);
    m_shader->Init(engine);

    EngineComponent::Create(engine, m_shader->GetShaderProgram(), &engine->GetInstance()->GetDescriptorPool());
}

void ComputePipeline::Destroy(Engine *engine)
{
    EngineComponent::Destroy(engine);
}

} // namespace hyperion::v2