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

    auto *env = Environment::GetInstance();
    if (env->ShadowsEnabled()) {
        for (int i = 0; i < env->NumCascades(); i++) {
            const std::string i_str = std::to_string(i);

            if (auto shadow_map = env->GetShadowMap(i)) {
                shadow_map->Prepare();

                SetUniform("u_shadowMap[" + i_str + "]", shadow_map.get());
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
        cubemap->Prepare();

        SetUniform("env_GlobalCubemap", cubemap.get());

        if (env->ProbeEnabled()) {
            const auto &origin = env->GetProbeRenderer()->GetProbe()->GetOrigin();
            SetUniform("EnvProbe.position", origin);
            SetUniform("EnvProbe.max", Vector3(env->GetProbeRenderer()->GetProbe()->GetFar()));
            SetUniform("EnvProbe.min", Vector3(-env->GetProbeRenderer()->GetProbe()->GetFar()));
        }
    }

    if (auto cubemap = env->GetGlobalIrradianceCubemap()) {
        cubemap->Prepare();

        SetUniform("env_GlobalIrradianceCubemap", cubemap.get());
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
