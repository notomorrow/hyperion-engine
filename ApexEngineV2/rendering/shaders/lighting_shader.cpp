#include "lighting_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace apex {
LightingShader::LightingShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/default.vert");
    const std::string fs_path("res/shaders/default.frag");

    AddSubShader(SubShader(CoreEngine::VERTEX_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
            properties, vs_path)
        ));

    AddSubShader(SubShader(CoreEngine::FRAGMENT_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
            properties, fs_path)
        ));

    for (int i = 0; i < 16; i++) {
        SetUniform("poissonDisk[" + std::to_string(i) + "]", 
            Environment::possion_disk[i]);
    }
}

void LightingShader::ApplyMaterial(const Material &mat)
{
    auto *env = Environment::GetInstance();
    if (env->ShadowsEnabled()) {
        if (env->NumCascades() == 1) {
            Texture::ActiveTexture(5);
            env->GetShadowMap(0)->Use();
            SetUniform("u_shadowMap", 5);
            SetUniform("u_shadowMatrix", env->GetShadowMatrix(0));
        }
    }

    env->GetSun().Bind(0, this);

    if (mat.diffuse_texture != nullptr) {
        Texture::ActiveTexture(0);
        mat.diffuse_texture->Use();
        SetUniform("u_diffuseTexture", 0);
    }
}

void LightingShader::ApplyTransforms(const Matrix4 &model, const Matrix4 &view, const Matrix4 &proj)
{
    Shader::ApplyTransforms(model, view, proj);
}
}