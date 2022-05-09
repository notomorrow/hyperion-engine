#include "shader.h"
#include "../engine.h"

namespace hyperion::v2 {
Shader::Shader(const std::vector<SubShader> &sub_shaders)
    : EngineComponentBase(),
      m_shader_program(std::make_unique<ShaderProgram>()),
      m_sub_shaders(sub_shaders)
{
}

Shader::~Shader()
{
    Teardown();
}

void Shader::Init(Engine *engine)
{
    if (IsInit()) {
        return;
    }

    EngineComponentBase::Init();

    OnInit(engine->callbacks.Once(EngineCallback::CREATE_SHADERS, [this](Engine *engine) {

        engine->render_scheduler.Enqueue([this, engine] {
            for (const auto &sub_shader : m_sub_shaders) {
                HYPERION_BUBBLE_ERRORS(
                    m_shader_program->AttachShader(
                        engine->GetInstance()->GetDevice(),
                        sub_shader.type,
                        sub_shader.spirv
                    )
                );
            }

            return m_shader_program->Create(engine->GetDevice());
        });

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SHADERS, [this](Engine *engine) {
            engine->render_scheduler.Enqueue([this, engine] {
                return m_shader_program->Destroy(engine->GetDevice());
            });
            
            engine->render_scheduler.FlushOrWait([](auto &fn) {
                HYPERION_ASSERT_RESULT(fn());
            });
        }), engine);
    }));
}

} // namespace hyperion