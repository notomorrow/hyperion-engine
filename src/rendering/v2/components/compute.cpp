#include "compute.h"
#include "../engine.h"

namespace hyperion::v2 {
ComputePipeline::ComputePipeline(Shader::ID shader_id)
    : BaseComponent(std::make_unique<renderer::ComputePipeline>()),
      m_shader_id(shader_id)
{
}

ComputePipeline::~ComputePipeline() = default;

void ComputePipeline::Create(Engine *engine)
{
    auto *shader = engine->GetShader(m_shader_id);
    AssertThrow(shader != nullptr);

    auto result = m_wrapped->Create(engine->GetInstance()->GetDevice(), shader->GetWrappedObject(), &engine->GetInstance()->GetDescriptorPool());
    AssertThrowMsg(result, "%s", result.message);
}

void ComputePipeline::Destroy(Engine *engine)
{
    auto result = m_wrapped->Destroy(engine->GetInstance()->GetDevice());
    AssertThrowMsg(result, "%s", result.message);
}

void ComputePipeline::Dispatch(Engine *engine, CommandBuffer *command_buffer, size_t num_groups_x, size_t num_groups_y, size_t num_groups_z)
{
    m_wrapped->Bind(command_buffer->GetCommandBuffer());
    engine->GetInstance()->GetDescriptorPool().BindDescriptorSets(command_buffer, m_wrapped.get());
    m_wrapped->Dispatch(command_buffer->GetCommandBuffer(), num_groups_x, num_groups_y, num_groups_z);
}
} // namespace hyperion