#include "shader.h"
#include "../engine.h"

namespace hyperion::v2 {
Shader::Shader(const std::vector<SpirvObject> &spirv_objects)
    : BaseComponent(std::make_unique<renderer::Shader>()),
      m_spirv_objects(spirv_objects)
{
}

Shader::~Shader() = default;

void Shader::Create(Engine *engine)
{
    for (const auto &spriv : m_spirv_objects) {
        m_wrapped->AttachShader(engine->GetInstance()->GetDevice(), spriv);
    }

    m_wrapped->CreateProgram("main");
}

void Shader::Destroy(Engine *engine)
{
    m_wrapped->Destroy();
}
} // namespace hyperion