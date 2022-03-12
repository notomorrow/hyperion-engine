#include "pipeline.h"
#include "../engine.h"

namespace hyperion::v2 {

Pipeline::Pipeline(renderer::Pipeline::ConstructionInfo &&construction_info)
    : BaseComponent(std::make_unique<renderer::Pipeline>(std::move(construction_info)))
{
}

Pipeline::~Pipeline() = default;

void Pipeline::Create(Engine *engine)
{
    m_wrapped->Build(engine->GetInstance()->GetDevice(), &engine->GetInstance()->GetDescriptorPool());
}

void Pipeline::Destroy(Engine *engine)
{
    m_wrapped->Destroy(engine->GetInstance()->GetDevice());
    m_wrapped.reset();
}

} // namespace hyperion::v2