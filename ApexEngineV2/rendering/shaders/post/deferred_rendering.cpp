#include "deferred_rendering.h"
#include "../../environment.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace apex {
DeferredRenderingShader::DeferredRenderingShader(const ShaderProperties &properties)
    : PostShader(properties)
{
    const std::string fs_path("res/shaders/filters/deferred.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );

    for (int i = 0; i < 16; i++) {
        SetUniform("poissonDisk[" + std::to_string(i) + "]",
            Environment::possion_disk[i]);
    }
}

void DeferredRenderingShader::ApplyMaterial(const Material &mat)
{
    PostShader::ApplyMaterial(mat);

    int texture_index = 6;

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
}

void DeferredRenderingShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("CameraPosition", camera->GetTranslation());

    Matrix4 inverse_proj_view(camera->GetViewMatrix());
    inverse_proj_view *= camera->GetProjectionMatrix();
    inverse_proj_view.Invert();

    SetUniform("InverseViewProjMatrix", inverse_proj_view);
}
} // namespace apex
