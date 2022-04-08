#include "shader.h"
#include "../engine.h"

namespace hyperion::v2 {
Shader::Shader(EngineCallbacks &callbacks, const std::vector<SubShader> &sub_shaders)
    : EngineComponent(callbacks),
      m_sub_shaders(sub_shaders)
{
}

Shader::~Shader() = default;

void Shader::Create(Engine *engine)
{
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

    AssertThrowMsg(create_shader_result, "%s", create_shader_result.message);

    EngineComponent::Create(engine);
}

void Shader::Destroy(Engine *engine)
{
    EngineComponent::Destroy(engine);
}
} // namespace hyperion