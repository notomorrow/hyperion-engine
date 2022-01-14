#include "bloom.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace apex {
BloomShader::BloomShader(const ShaderProperties &properties)
    : PostShader(properties)
{
    const std::string fs_path("res/shaders/filters/bloom.frag");

    AddSubShader(SubShader(Shader::SubShaderType::SUBSHADER_FRAGMENT,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
            properties, fs_path
        )
    ));
}

void BloomShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
}
} // namespace apex
