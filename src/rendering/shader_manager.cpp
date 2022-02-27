#include "shader_manager.h"
#include "environment.h"

namespace hyperion {

ShaderManager *ShaderManager::instance = nullptr;

ShaderManager *ShaderManager::GetInstance()
{
    if (instance == nullptr) {
        instance = new ShaderManager();
    }

    return instance;
}

ShaderManager::ShaderManager()
{
    m_base_shader_properties
        .Define("DEFERRED", true)
        .Define("GI_INTENSITY", 20.0f)
        .Define("IBL_INTENSITY", 8000.0f)
        .Define("SKY_INTENSITY", 100000.0f)
        .Define("METALNESS_MAPPING", true)
        .Define("ROUGHNESS_MAPPING", true)
        .Define("NORMAL_MAPPING", true)
        .Define("SSR_ENABLED", false)
        .Define("MAX_POINT_LIGHTS", int(Environment::max_point_lights_on_screen))
        .Define("HDR", true)
        .Define("HDR_TONEMAP_FILMIC", true)
        .Define("HDR_TONEMAP_UNREAL", false);
}

ShaderManager::~ShaderManager()
{
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
