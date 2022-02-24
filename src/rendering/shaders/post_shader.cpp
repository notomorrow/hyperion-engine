#include "post_shader.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace hyperion {
PostShader::PostShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/post.vert");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_VERTEX,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
        properties,
        vs_path
    );
}

void PostShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);
}
} // namespace hyperion
