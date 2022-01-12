#include "ui_object_shader.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace apex {
UIObjectShader::UIObjectShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/ui/ui_object.vert");

    AddSubShader(SubShader(Shader::SubShaderType::SUBSHADER_VERTEX,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
            properties, vs_path)
        ));

    const std::string fs_path("res/shaders/ui/ui_object.frag");

    AddSubShader(SubShader(Shader::SubShaderType::SUBSHADER_FRAGMENT,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
            properties, fs_path)
        ));
}

void UIObjectShader::ApplyMaterial(const Material &mat)
{
    int texture_index = 1;

    // if (mat.texture0 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture0->Use();
    //     SetUniform("u_colorMap", texture_index);

    //     texture_index++;
    // }

    // if (mat.texture1 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture1->Use();
    //     SetUniform("u_depthMap", texture_index);

    //     texture_index++;
    // }

    // if (mat.texture2 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture2->Use();
    //     SetUniform("u_positionMap", texture_index);

    //     texture_index++;
    // }

    // if (mat.normals0 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.normals0->Use();
    //     SetUniform("u_normalMap", texture_index);

    //     texture_index++;
    // }

    for (auto it = mat.textures.begin(); it != mat.textures.end(); it++) {
        Texture::ActiveTexture(texture_index);
        it->second->Use();
        SetUniform(it->first, texture_index);
        SetUniform(std::string("Has") + it->first, 1);
        texture_index++;
    }

    // if (mat.alpha_blended) {
    //     glEnable(GL_BLEND);
    //     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // }

    //glDepthMask(false);
    //glDisable(GL_DEPTH_TEST);
}

void UIObjectShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);

    Transform model_2d;
    
    Vector2 screen_scale(1.0f / camera->GetWidth(), 1.0f / camera->GetHeight());
    model_2d.SetScale(Vector3(
        screen_scale.x * transform.GetScale().x,
        screen_scale.y * transform.GetScale().y,
        1.0
    ));

    model_2d.SetTranslation(transform.GetTranslation());

    // model_2d.SetTranslation(Vector3(
    //     ((transform.GetTranslation().x * screen_scale.x) + (model_2d.GetScale().x / 2.0)),
    //     ((transform.GetTranslation().y * screen_scale.y) + (model_2d.GetScale().y / 2.0)),
    //     0.0
    // ));

    // model_2d.SetRotation(transform.GetRotation());

    Vector2 viewport(camera->GetWidth(), camera->GetHeight());

    SetUniform("Viewport", viewport);
    SetUniform("u_modelMatrix", model_2d.GetMatrix());
}
} // namespace apex
