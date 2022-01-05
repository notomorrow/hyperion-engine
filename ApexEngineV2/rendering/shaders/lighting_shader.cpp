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
    const std::string fs_path("res/shaders/default_fresnel.frag");

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

void LightingShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    int texture_index = 1;

    auto *env = Environment::GetInstance();
    if (env->ShadowsEnabled()) {
        for (int i = 0; i < env->NumCascades(); i++) {
            const std::string i_str = std::to_string(i);

            if (auto shadow_map = env->GetShadowMap(i)) {
                Texture::ActiveTexture(texture_index);
                shadow_map->Use();
                SetUniform("u_shadowMap[" + i_str + "]", texture_index);
                texture_index++;
            }

            SetUniform("u_shadowMatrix[" + i_str + "]", env->GetShadowMatrix(i));
            SetUniform("u_shadowSplit[" + i_str + "]", (float)env->GetShadowSplit(i));
        }
    }

    env->GetSun().Bind(0, this);

    SetUniform("env_NumPointLights", (int)env->GetNumPointLights());

    for (int i = 0; i < env->GetNumPointLights(); i++) {
        if (auto point_light = env->GetPointLight(i)) {
            point_light->Bind(i, this);
        }
    }

    SetUniform("u_diffuseColor", mat.diffuse_color);

    // if (mat.texture0 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture0->Use();
    //     SetUniform("DiffuseMap", texture_index);
    //     SetUniform("HasDiffuseMap", 1);

    //     texture_index++;
    // } else {
    //     SetUniform("HasDiffuseMap", 0);
    // }

    if (auto cubemap = env->GetGlobalCubemap()) {
        Texture::ActiveTexture(texture_index);
        cubemap->Use();
        SetUniform("env_GlobalCubemap", texture_index);

        texture_index++;
    }

    if (auto cubemap = env->GetGlobalIrradianceCubemap()) {
        Texture::ActiveTexture(texture_index);
        cubemap->Use();
        SetUniform("env_GlobalIrradianceCubemap", texture_index);

        texture_index++;
    }

    // if (mat.texture1 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture1->Use();
    //     SetUniform("ParallaxMap", texture_index);
    //     SetUniform("HasParallaxMap", 1);

    //     texture_index++;
    // } else {
    //     SetUniform("HasParallaxMap", 0);
    // }

    // if (mat.texture2 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture2->Use();
    //     SetUniform("AoMap", texture_index);
    //     SetUniform("HasAoMap", 1);

    //     texture_index++;
    // } else {
    //     SetUniform("HasAoMap", 0);
    // }

    // if (mat.texture3 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture3->Use();
    //     SetUniform("BrdfMap", texture_index);

    //     texture_index++;
    // }

    // if (mat.normals0 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.normals0->Use();
    //     SetUniform("NormalMap", texture_index);
    //     SetUniform("HasNormalMap", 1);

    //     texture_index++;
    // } else {
    //     SetUniform("HasNormalMap", 0);
    // }

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

    // if (properties["ROUGHNESS_MAPPING"] && mat.texture4 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture4->Use();
    //     SetUniform("u_roughnessMap", texture_index);
    //     SetUniform("u_hasRoughnessMap", 1);
    //     texture_index++;
    // }

    // if (properties["METALNESS_MAPPING"] && mat.texture5 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.texture5->Use();
    //     SetUniform("u_metalnessMap", texture_index);
    //     SetUniform("u_hasMetalnessMap", 1);
    //     texture_index++;
    // }

    // if (mat.normals0 != nullptr) {
    //     Texture::ActiveTexture(texture_index);
    //     mat.normals0->Use();
    //     SetUniform("u_normalMap", texture_index);
    //     SetUniform("u_hasNormalMap", 1);

    //     texture_index++;
    // } else {
    //     SetUniform("u_hasNormalMap", 0);
    // }

    if (mat.HasParameter("shininess")) {
        SetUniform("u_shininess", mat.GetParameter("shininess")[0]);
    }

    if (mat.HasParameter("roughness")) {
        SetUniform("u_roughness", mat.GetParameter("roughness")[0]);
    }
}

void LightingShader::ApplyTransforms(const Matrix4 &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("u_camerapos", camera->GetTranslation());
}
} // namespace apex
