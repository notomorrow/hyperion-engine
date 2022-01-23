#include "lighting_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace hyperion {
LightingShader::LightingShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/default.vert");
    const std::string fs_path("res/shaders/default.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_VERTEX,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
        properties,
        vs_path
    );

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

void LightingShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

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

    SetUniform("u_diffuseColor", mat.diffuse_color);

    if (auto cubemap = env->GetGlobalCubemap()) {
        cubemap->Prepare();

        SetUniform("env_GlobalCubemap", cubemap.get());
    }

    if (auto cubemap = env->GetGlobalIrradianceCubemap()) {
        cubemap->Prepare();

        SetUniform("env_GlobalIrradianceCubemap", cubemap.get());
    }

    for (auto it = mat.textures.begin(); it != mat.textures.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }

        it->second->Prepare();

        SetUniform(it->first, it->second.get());
        SetUniform(std::string("Has") + it->first, 1);
    }

    if (mat.HasParameter("shininess")) {
        SetUniform("u_shininess", mat.GetParameter("shininess")[0]);
    }

    if (mat.HasParameter("roughness")) {
        SetUniform("u_roughness", mat.GetParameter("roughness")[0]);
    }

    if (mat.HasParameter("RimShading")) {
        SetUniform("RimShading", mat.GetParameter("RimShading")[0]);
    }

    if (mat.HasParameter("FlipUV")) {
        const auto &param = mat.GetParameter("FlipUV");
        SetUniform("FlipUV_X", int(param[0]));
        SetUniform("FlipUV_Y", int(param[1]));
    } else if (mat.HasParameter("FlipUV_X")) {
        SetUniform("FlipUV_X", int(mat.GetParameter("FlipUV_X")[0]));
    } else if (mat.HasParameter("FlipUV_Y")) {
        SetUniform("FlipUV_Y", int(mat.GetParameter("FlipUV_X")[0]));
    }
}

void LightingShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("u_camerapos", camera->GetTranslation());
}
} // namespace hyperion
