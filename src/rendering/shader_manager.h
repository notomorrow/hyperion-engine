#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H

#include "shader.h"

#include <map>
#include <vector>
#include <utility>
#include <string>

namespace hyperion {

class ShaderManager {
public:
    static ShaderManager *instance;
    static ShaderManager *GetInstance();

    ShaderManager();
    ~ShaderManager();

    template <typename T>
    std::shared_ptr<T> GetShader(const ShaderProperties &properties)
    {
        static_assert(std::is_base_of<Shader, T>::value,
            "T must be a derived class of Shader");

        static_assert(std::is_constructible<T, const ShaderProperties &>::value,
            "T must be constructable with: const ShaderProperties &");

        ShaderProperties provided_merged_with_base(m_base_shader_properties);
        provided_merged_with_base.Merge(properties);

        for (auto &&ins : instances) {
            auto ins_casted = std::dynamic_pointer_cast<T>(ins.first);

            if (ins_casted && ins.second == properties) {
                return ins_casted;
            }
        }

        auto new_ins = std::make_shared<T>(provided_merged_with_base);
        instances.push_back(std::make_pair(new_ins, properties));

        return new_ins;
    }

    const ShaderProperties &GetBaseShaderProperties() const { return m_base_shader_properties; }
    // merge values into the base instance. Updates all existing shaders.
    void SetBaseShaderProperties(const ShaderProperties &properties);

private:
    ShaderProperties m_base_shader_properties;

    std::vector<
        std::pair<
            std::shared_ptr<Shader>, 
            ShaderProperties
        >
    > instances;
};

} // namespace hyperion

#endif
