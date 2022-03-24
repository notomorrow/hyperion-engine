#ifndef HYPERION_V2_SHADER_H
#define HYPERION_V2_SHADER_H

#include "base.h"

#include <rendering/backend/renderer_shader.h>

#include <memory>

namespace hyperion::v2 {

using renderer::ShaderObject;
using renderer::ShaderModule;

struct SubShader {
    ShaderModule::Type type;
    ShaderObject       spirv;
};

class Shader : public EngineComponent<renderer::ShaderProgram> {
public:
    explicit Shader(const std::vector<SubShader> &sub_shaders);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    ~Shader();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    std::vector<SubShader> m_sub_shaders;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_SHADER_H

