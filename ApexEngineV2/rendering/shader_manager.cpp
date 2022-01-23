#include "shader_manager.h"

namespace hyperion {

ShaderManager *ShaderManager::instance = nullptr;

ShaderManager *ShaderManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new ShaderManager();
    }

    return instance;
}

void ShaderManager::SetBaseShaderProperties(const ShaderProperties &properties)
{
    m_base_shader_properties.Merge(properties);

    for (auto &it : instances) {
        if (it.first == nullptr) {
            continue;
        }

        const ShaderProperties &current = it.second;

        ShaderProperties updated(m_base_shader_properties);
        updated.Merge(current);

        it.first->SetProperties(updated);
        // NOTE: do not update it.second because that excludes base properties
    }
}

} // namespace hyperion
