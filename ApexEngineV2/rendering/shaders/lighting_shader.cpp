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

    if (mat.texture0 != nullptr) {
        Texture::ActiveTexture(texture_index);
        mat.texture0->Use();
        SetUniform("u_diffuseMap", texture_index);
        SetUniform("u_hasDiffuseMap", 1);

        texture_index++;
    } else {
        SetUniform("u_hasDiffuseMap", 0);
    }

    if (auto cubemap = env->GetGlobalCubemap()) {
        Texture::ActiveTexture(texture_index);
        cubemap->Use();
        SetUniform("env_GlobalCubemap", texture_index);

        texture_index++;
    }

    if (mat.texture1 != nullptr) {
        Texture::ActiveTexture(texture_index);
        mat.texture1->Use();
        SetUniform("u_parallaxMap", texture_index);
        SetUniform("u_hasParallaxMap", 1);

        texture_index++;
    } else {
        SetUniform("u_hasParallaxMap", 0);
    }

    if (mat.texture2 != nullptr) {
        Texture::ActiveTexture(texture_index);
        mat.texture2->Use();
        SetUniform("u_aoMap", texture_index);
        SetUniform("u_hasAoMap", 1);

        texture_index++;
    } else {
        SetUniform("u_hasAoMap", 0);
    }

    if (mat.texture3 != nullptr) {
        Texture::ActiveTexture(texture_index);
        mat.texture3->Use();
        SetUniform("u_brdfMap", texture_index);

        texture_index++;
    }

    if (mat.normals0 != nullptr) {
        Texture::ActiveTexture(texture_index);
        mat.normals0->Use();
        SetUniform("u_normalMap", texture_index);
        SetUniform("u_hasNormalMap", 1);

        texture_index++;
    } else {
        SetUniform("u_hasNormalMap", 0);
    }

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
