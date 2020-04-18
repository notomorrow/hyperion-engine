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
    int texture_index = 1;

    if (mat.texture0 != nullptr) {
        Texture::ActiveTexture(texture_index);
        mat.texture0->Use();
        SetUniform("u_colorMap", texture_index);

        texture_index++;
    }

    if (mat.texture1 != nullptr) {
        Texture::ActiveTexture(texture_index);
        mat.texture1->Use();
        SetUniform("u_depthMap", texture_index);

        texture_index++;
    }

    if (mat.texture2 != nullptr) {
        Texture::ActiveTexture(texture_index);
        mat.texture2->Use();
        SetUniform("u_positionMap", texture_index);

        texture_index++;
    }

    if (mat.normals0 != nullptr) {
        Texture::ActiveTexture(texture_index);
        mat.normals0->Use();
        SetUniform("u_normalMap", texture_index);

        texture_index++;
    }

    // if (mat.alpha_blended) {
    //     glEnable(GL_BLEND);
    //     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // }

    //glDepthMask(false);
    //glDisable(GL_DEPTH_TEST);
}
} // namespace apex
