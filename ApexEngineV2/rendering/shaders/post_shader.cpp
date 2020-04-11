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
    if (mat.texture0 != nullptr) {
        Texture::ActiveTexture(0);
        mat.texture0->Use();
        SetUniform("u_colorMap", 0);
    }

    if (mat.texture1 != nullptr) {
        Texture::ActiveTexture(1);
        mat.texture1->Use();
        SetUniform("u_depthMap", 1);
    }

    if (mat.texture2 != nullptr) {
        Texture::ActiveTexture(2);
        mat.texture2->Use();
        SetUniform("u_positionMap", 2);
    }

    if (mat.normals0 != nullptr) {
        Texture::ActiveTexture(3);
        mat.normals0->Use();
        SetUniform("u_normalMap", 3);
    }

    // if (mat.alpha_blended) {
    //     glEnable(GL_BLEND);
    //     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // }

    glDepthMask(false);
    glDisable(GL_DEPTH_TEST);
}
} // namespace apex
