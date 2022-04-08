#include "shader.h"
#include "../engine.h"

namespace hyperion::v2 {
Shader::Shader(const std::vector<SubShader> &sub_shaders)
    : EngineComponent(),
      m_sub_shaders(sub_shaders)
{
}

Shader::~Shader()
{
    Teardown();
}

void Shader::Init(Engine *engine)
{
    Track(engine->callbacks.Once(EngineCallback::CREATE_SHADERS, [this](Engine *engine) {
        auto create_shader_result = renderer::Result::OK;

        for (const auto &sub_shader : m_sub_shaders) {
            HYPERION_PASS_ERRORS(
                m_wrapped.AttachShader(
                    engine->GetInstance()->GetDevice(),
                    sub_shader.type,
                    sub_shader.spirv
                ),
                create_shader_result
            );
        }

        HYPERION_ASSERT_RESULT(create_shader_result);

        EngineComponent::Create(engine);

        OnTeardown(engine->callbacks.Once(EngineCallback::DESTROY_SHADERS, [this](Engine *engine) {
            EngineComponent::Destroy(engine);
        }), engine);
    }));

}

void Shader::Destroy(Engine *engine)
{
}
} // namespace hyperion