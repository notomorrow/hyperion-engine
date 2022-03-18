#include "shader.h"
#include "../engine.h"

namespace hyperion::v2 {
Shader::Shader(const std::vector<SpirvObject> &spirv_objects)
    : EngineComponent(),
      m_spirv_objects(spirv_objects)
{
}

Shader::~Shader() = default;

void Shader::Create(Engine *engine)
{
    for (const auto &spriv : m_spirv_objects) {
        m_wrapped.AttachShader(engine->GetInstance()->GetDevice(), spriv);
    }

    m_wrapped.CreateProgram("main");

    m_is_created = true;
}

void Shader::Destroy(Engine *engine)
{
    EngineComponent::Destroy(engine);
}
} // namespace hyperion