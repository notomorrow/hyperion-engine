#include "terrain_shader.h"
#include "../rendering/environment.h"
#include "../asset/asset_manager.h"
#include "../asset/text_loader.h"
#include "../util/shader_preprocessor.h"

namespace hyperion {
TerrainShader::TerrainShader(const ShaderProperties &properties)
    : LightingShader(properties)
{
    const std::string fs_path("shaders/terrain.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );

    m_uniform_base_terrain_scale = m_uniforms.Acquire("BaseTerrainScale").id;

    SetUniform(m_uniform_base_terrain_scale, 0.85f);
}

void TerrainShader::ApplyMaterial(const Material &mat)
{
    LightingShader::ApplyMaterial(mat);
}
} // namespace hyperion
