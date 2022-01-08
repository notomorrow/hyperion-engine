#include "skybox_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace apex {
SkyboxShader::SkyboxShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/skybox.vert");
    const std::string fs_path("res/shaders/skybox.frag");

    AddSubShader(SubShader(GL_VERTEX_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
            properties, vs_path)
        )
    );

    AddSubShader(SubShader(GL_FRAGMENT_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
            properties, fs_path)
        )
    );
}

void SkyboxShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    int texture_index = 1;

    for (auto it = mat.textures.begin(); it != mat.textures.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }

        Texture::ActiveTexture(texture_index);
        it->second->Use();
        SetUniform(it->first, texture_index);
        SetUniform(std::string("Has") + it->first, 1);

        texture_index++;
    }
}

void SkyboxShader::ApplyTransforms(const Matrix4 &transform, Camera *camera)
{
    Matrix4 model_mat = transform;
    model_mat(0, 3) = camera->GetTranslation().x;
    model_mat(1, 3) = camera->GetTranslation().y;
    model_mat(2, 3) = camera->GetTranslation().z;

    Shader::ApplyTransforms(model_mat, camera);

    SetUniform("u_camerapos", camera->GetTranslation());
}
} // namespace apex
