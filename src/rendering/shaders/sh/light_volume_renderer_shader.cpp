#include "light_volume_renderer_shader.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace hyperion {
LightVolumeRendererShader::LightVolumeRendererShader(const ShaderProperties &properties)
    : CubemapRendererShader(properties)
{
    const std::string fs_path("shaders/gi/light_volume_cubemap.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );
}
} // namespace hyperion
