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
    int texture_index = 1;

    auto *env = Environment::GetInstance();
    if (env->ShadowsEnabled()) {
        for (int i = 0; i < env->NumCascades(); i++) {
            std::string i_str = std::to_string(i);
            Texture::ActiveTexture(texture_index);
            env->GetShadowMap(i)->Use();
            SetUniform("u_shadowMap[" + i_str + "]", texture_index);
            SetUniform("u_shadowMatrix[" + i_str + "]", env->GetShadowMatrix(i));
            SetUniform("u_shadowSplit[" + i_str + "]", (float)env->GetShadowSplit(i));

            texture_index++;
        }
    }

    env->GetSun().Bind(0, this);

    // if (mat.texture0 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture0->Use();
    //     SetUniform("terrainTexture0", texture_index);
    //     SetUniform("terrainTexture0Scale", 40.0f);

    //     texture_index++;
    // }

    // if (mat.normals0 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.normals0->Use();
    //     SetUniform("terrainTexture0Normal", texture_index);

    //     texture_index++;
    // }

    // if (mat.texture1 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture1->Use();
    //     SetUniform("slopeTexture", texture_index);
    //     SetUniform("slopeTextureScale", 20.0f);

    //     texture_index++;
    // }

    // if (mat.normals1 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.normals1->Use();
    //     SetUniform("slopeTextureNormal", texture_index);

    //     texture_index++;
    // }

    if (auto cubemap = env->GetGlobalCubemap()) {
        Texture::ActiveTexture(texture_index);
        cubemap->Use();
        SetUniform("env_GlobalCubemap", texture_index);

        texture_index++;
    }

    // if (mat.texture2 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture2->Use();
    //     SetUniform("u_brdfMap", texture_index);

    //     texture_index++;
    // }

    for (auto it = mat.textures.begin(); it != mat.textures.end(); it++) {
        Texture::ActiveTexture(texture_index);
        it->second->Use();
        SetUniform(it->first, texture_index);
        SetUniform(std::string("Has") + it->first, 1);

        texture_index++;
    }

    if (mat.HasParameter("shininess")) {
        SetUniform("u_shininess", mat.GetParameter("shininess")[0]);
    }

    if (mat.HasParameter("roughness")) {
        SetUniform("u_roughness", mat.GetParameter("roughness")[0]);
    }
}

void TerrainShader::ApplyTransforms(const Matrix4 &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("u_camerapos", camera->GetTranslation());
}
} // namespace apex
