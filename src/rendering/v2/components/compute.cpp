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
    AssertThrow(m_shader != nullptr);
    m_shader->Init(engine);

    HYPERION_ASSERT_RESULT(m_wrapped.Create(
        engine->GetInstance()->GetDevice(),
        &m_shader->Get(),
        &engine->GetInstance()->GetDescriptorPool()
    ));

    m_is_created = true;
}

void ComputePipeline::Destroy(Engine *engine)
{
    EngineComponent::Destroy(engine);
}

/*void ComputePipeline::Dispatch(Engine *engine,
                               CommandBuffer *command_buffer,
                               Extent3D group_size)
{
    m_wrapped.Bind(command_buffer);

    BindDescriptorSets(engine, command_buffer);

    m_wrapped.Dispatch(command_buffer, group_size);
}

void ComputePipeline::DispatchIndirect(Engine *engine,
                                       CommandBuffer *command_buffer,
                                       IndirectBuffer *indirect,
                                       size_t offset)
{
    m_wrapped.Bind(command_buffer);


}

void ComputePipeline::BindDescriptorSets(Engine *engine,
                                         CommandBuffer *command_buffer)
{
    engine->GetInstance()->GetDescriptorPool().Bind(
        engine->GetInstance()->GetDevice(),
        command_buffer,
        &m_wrapped,
        {{
            .set = DescriptorSet::DESCRIPTOR_SET_INDEX_GLOBAL,
            .count = 1
        }}
    );
}*/

} // namespace hyperion::v2