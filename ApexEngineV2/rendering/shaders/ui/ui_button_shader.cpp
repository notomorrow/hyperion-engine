#include "ui_button_shader.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace apex {
UIButtonShader::UIButtonShader(const ShaderProperties &properties)
    : UIObjectShader(properties)
{
    const std::string vs_path("res/shaders/ui/ui_object.vert");

    AddSubShader(SubShader(Shader::SubShaderType::SUBSHADER_VERTEX,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
            properties, vs_path)
        ));

    const std::string fs_path("res/shaders/ui/ui_button.frag");

    AddSubShader(SubShader(Shader::SubShaderType::SUBSHADER_FRAGMENT,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
            properties, fs_path)
        ));
}
} // namespace apex
