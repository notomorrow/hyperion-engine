#include "shader.h"

namespace hyperion::v2 {
Shader::Shader(const std::vector<SpirvObject> &spirv_objects)
    : BaseComponent(std::make_unique<renderer::Shader>()),
      m_spirv_objects(spirv_objects)
{
}

Shader::~Shader() = default;

void Shader::Create(Instance *instance)
{
    for (const auto &spriv : m_spirv_objects) {
        m_wrapped->AttachShader(instance->GetDevice(), spriv);
    }

    m_wrapped->CreateProgram("main");
}

void Shader::Destroy(Instance *instance)
{
    m_wrapped->Destroy();
}
} // namespace hyperion