#include "terrain_shader.h"
#include "../rendering/environment.h"
#include "../asset/asset_manager.h"
#include "../asset/text_loader.h"
#include "../util/shader_preprocessor.h"

namespace apex {
TerrainShader::TerrainShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/terrain.vert");
    const std::string fs_path("res/shaders/terrain.frag");

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

    for (int i = 0; i < 16; i++) {
        SetUniform("poissonDisk[" + std::to_string(i) + "]",
            Environment::possion_disk[i]);
    }
}

void TerrainShader::ApplyMaterial(const Material &mat)
{
    auto *env = Environment::GetInstance();
    if (env->ShadowsEnabled()) {
        for (int i = 0; i < env->NumCascades(); i++) {
            std::string i_str = std::to_string(i);
            Texture::ActiveTexture(5 + i);
            env->GetShadowMap(i)->Use();
            SetUniform("u_shadowMap[" + i_str + "]", 5 + i);
            SetUniform("u_shadowMatrix[" + i_str + "]", env->GetShadowMatrix(i));
        }
    }

    env->GetSun().Bind(0, this);

    if (mat.texture0 != nullptr) {
        Texture::ActiveTexture(0);
        mat.texture0->Use();
        SetUniform("terrainTexture0", 0);
        SetUniform("terrainTexture0Scale", 20.0f);
    }

    if (mat.normals0 != nullptr) {
        Texture::ActiveTexture(1);
        mat.normals0->Use();
        SetUniform("terrainTexture0Normal", 1);
    }

    if (mat.texture1 != nullptr) {
        Texture::ActiveTexture(2);
        mat.texture1->Use();
        SetUniform("slopeTexture", 2);
        SetUniform("slopeTextureScale", 20.0f);
    }

    if (mat.normals1 != nullptr) {
        Texture::ActiveTexture(3);
        mat.normals1->Use();
        SetUniform("slopeTextureNormal", 3);
    }
}

void TerrainShader::ApplyTransforms(const Matrix4 &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("u_camerapos", camera->GetTranslation());
}
} // namespace apex