#include "compute.h"
#include "../engine.h"

namespace hyperion::v2 {
ComputePipeline::ComputePipeline(Ref<Shader> &&shader)
    : EngineComponentBase(),
      m_pipeline(std::make_unique<renderer::ComputePipeline>()),
      m_shader(std::move(shader))
{
}

ComputePipeline::~ComputePipeline()
{
    Teardown();
}

void ComputePipeline::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_COMPUTE_PIPELINES, [this](Engine *engine) {
        AssertThrow(m_shader != nullptr);
        m_shader->Init(engine);

        engine->render_scheduler.Enqueue([this, engine] {
           return m_pipeline->Create(
               engine->GetDevice(),
               m_shader->GetShaderProgram(),
               &engine->GetInstance()->GetDescriptorPool()
           ); 
        });

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_COMPUTE_PIPELINES, [this](Engine *engine) {
            engine->render_scheduler.Enqueue([this, engine] {
               return m_pipeline->Destroy(engine->GetDevice()); 
            });
            
            engine->render_scheduler.FlushOrWait([](auto &fn) {
                HYPERION_ASSERT_RESULT(fn());
            });
        }), engine);
    }));
}

} // namespace hyperion::v2