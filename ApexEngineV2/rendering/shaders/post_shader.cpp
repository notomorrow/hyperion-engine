#include "post_shader.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace apex {
PostShader::PostShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/post.vert");

    AddSubShader(SubShader(GL_VERTEX_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
            properties, vs_path)
        ));
}

void PostShader::ApplyMaterial(const Material &mat)
{
    if (mat.diffuse_texture != nullptr) {
        Texture::ActiveTexture(0);
        mat.diffuse_texture->Use();
        SetUniform("u_texture", 0);
    }
    if (mat.alpha_blended) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
}
} // namespace apex