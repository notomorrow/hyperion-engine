#ifndef HYPERION_V2_SHADER_H
#define HYPERION_V2_SHADER_H

#include "base.h"

#include <rendering/backend/renderer_shader.h>

#include <memory>

namespace hyperion::v2 {

using renderer::SpirvObject;

class Shader : public EngineComponent<renderer::ShaderProgram> {
public:
    struct SubShader {
        SpirvObject::Type type;
        std::string       code;
    };

    class Builder {
    public:

        Builder &AddSubShader(const SubShader &sub_shader)
        {
            m_sub_shaders.push_back(sub_shader);

            return *this;
        }

        Builder &AddSpirv(const SpirvObject &spirv)
        {
            m_spirv_objects.push_back(spirv);

            return *this;
        }

        std::unique_ptr<Shader> Build()
        {
            // TODO: process ShaderProperties on any subshaders
            // TODO: compile any subshaders from GLSL -> SPIRV

            std::vector<SpirvObject> spirv_objects(std::move(m_spirv_objects));

            return std::make_unique<Shader>(spirv_objects);
        }

    private:
        std::vector<SubShader> m_sub_shaders;
        std::vector<SpirvObject> m_spirv_objects;
    };

    explicit Shader(const std::vector<SpirvObject> &spirv_objects);
    Shader(const Shader &) = delete;
    Shader &operator=(const Shader &) = delete;
    ~Shader();

    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    std::vector<SpirvObject> m_spirv_objects;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_SHADER_H

