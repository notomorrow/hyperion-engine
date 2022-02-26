#include "deferred_rendering.h"
#include "../../environment.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"

namespace hyperion {
DeferredRenderingShader::DeferredRenderingShader(const ShaderProperties &properties)
    : PostShader(properties)
{
    const std::string fs_path("shaders/filters/deferred.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );

    m_uniform_camera_position = m_uniforms.Acquire("CameraPosition").id;
    m_uniform_env_global_cubemap = m_uniforms.Acquire("env_GlobalCubemap").id;

    m_uniform_shadow_map[0] = m_uniforms.Acquire("u_shadowMap[0]").id;
    m_uniform_shadow_map[1] = m_uniforms.Acquire("u_shadowMap[1]").id;
    m_uniform_shadow_map[2] = m_uniforms.Acquire("u_shadowMap[2]").id;
    m_uniform_shadow_map[3] = m_uniforms.Acquire("u_shadowMap[3]").id;

    m_uniform_shadow_matrix[0] = m_uniforms.Acquire("u_shadowMatrix[0]").id;
    m_uniform_shadow_matrix[1] = m_uniforms.Acquire("u_shadowMatrix[1]").id;
    m_uniform_shadow_matrix[2] = m_uniforms.Acquire("u_shadowMatrix[2]").id;
    m_uniform_shadow_matrix[3] = m_uniforms.Acquire("u_shadowMatrix[3]").id;

    m_uniform_shadow_split[0] = m_uniforms.Acquire("u_shadowSplit[0]").id;
    m_uniform_shadow_split[1] = m_uniforms.Acquire("u_shadowSplit[1]").id;
    m_uniform_shadow_split[2] = m_uniforms.Acquire("u_shadowSplit[2]").id;
    m_uniform_shadow_split[3] = m_uniforms.Acquire("u_shadowSplit[3]").id;

    m_uniform_poisson_disk[0] = m_uniforms.Acquire("poissonDisk[0]").id;
    m_uniform_poisson_disk[1] = m_uniforms.Acquire("poissonDisk[1]").id;
    m_uniform_poisson_disk[2] = m_uniforms.Acquire("poissonDisk[2]").id;
    m_uniform_poisson_disk[3] = m_uniforms.Acquire("poissonDisk[3]").id;
    m_uniform_poisson_disk[4] = m_uniforms.Acquire("poissonDisk[4]").id;
    m_uniform_poisson_disk[5] = m_uniforms.Acquire("poissonDisk[5]").id;
    m_uniform_poisson_disk[6] = m_uniforms.Acquire("poissonDisk[6]").id;
    m_uniform_poisson_disk[7] = m_uniforms.Acquire("poissonDisk[7]").id;
    m_uniform_poisson_disk[8] = m_uniforms.Acquire("poissonDisk[8]").id;
    m_uniform_poisson_disk[9] = m_uniforms.Acquire("poissonDisk[9]").id;
    m_uniform_poisson_disk[10] = m_uniforms.Acquire("poissonDisk[10]").id;
    m_uniform_poisson_disk[11] = m_uniforms.Acquire("poissonDisk[11]").id;
    m_uniform_poisson_disk[12] = m_uniforms.Acquire("poissonDisk[12]").id;
    m_uniform_poisson_disk[13] = m_uniforms.Acquire("poissonDisk[13]").id;
    m_uniform_poisson_disk[14] = m_uniforms.Acquire("poissonDisk[14]").id;
    m_uniform_poisson_disk[15] = m_uniforms.Acquire("poissonDisk[15]").id;

    for (int i = 0; i < 16; i++) {
        SetUniform(m_uniform_poisson_disk[i], Environment::possion_disk[i]);
    }

    m_uniform_envmap_origin = m_uniforms.Acquire("EnvProbe.position").id;
    m_uniform_envmap_max = m_uniforms.Acquire("EnvProbe.max").id;
    m_uniform_envmap_min = m_uniforms.Acquire("EnvProbe.min").id;

    m_uniform_voxel_map = m_uniforms.Acquire("VoxelMap").id;
    m_uniform_voxel_scene_scale = m_uniforms.Acquire("VoxelSceneScale").id;
    m_uniform_voxel_probe_position = m_uniforms.Acquire("VoxelProbePosition").id;

    m_uniform_sh_map = m_uniforms.Acquire("SphericalHarmonicsMap").id;
    m_uniform_has_sh_map = m_uniforms.Acquire("HasSphericalHarmonicsMap").id;

    m_uniform_inverse_view_proj_matrix = m_uniforms.Acquire("InverseViewProjMatrix").id;
}

void DeferredRenderingShader::ApplyMaterial(const Material &mat)
{
    PostShader::ApplyMaterial(mat);

    auto *env = Environment::GetInstance();
    Shader::SetLightUniforms(env);

    if (env->ShadowsEnabled()) {
        for (int i = 0; i < env->NumCascades(); i++) {
            if (auto shadow_map = env->GetShadowMap(i)) {
                shadow_map->Prepare();

                SetUniform(m_uniform_shadow_map[i], shadow_map.get());
            }

            SetUniform(m_uniform_shadow_matrix[i], env->GetShadowMatrix(i));
            SetUniform(m_uniform_shadow_split[i], (float)env->GetShadowSplit(i));
        }
    }

    for (int i = 0; i < env->GetProbeManager()->NumProbes(); i++) {
        const auto &probe = env->GetProbeManager()->GetProbe(i);

        switch (probe->GetProbeType()) {
        case Probe::PROBE_TYPE_ENVMAP:
            if (auto &texture = probe->GetTexture()) {
                texture->Prepare();
                SetUniform(m_uniform_env_global_cubemap, texture.get());

                SetUniform(m_uniform_envmap_origin, probe->GetOrigin());
                SetUniform(m_uniform_envmap_max, probe->GetBounds().GetMax());
                SetUniform(m_uniform_envmap_min, probe->GetBounds().GetMin());
            }
            break;
        case Probe::PROBE_TYPE_VCT:
            if (auto &texture = probe->GetTexture()) {
                SetUniform(m_uniform_voxel_probe_position, probe->GetOrigin());
                SetUniform(m_uniform_voxel_scene_scale, probe->GetBounds().GetDimensions());
                SetUniform(m_uniform_voxel_map, texture.get());
            }
            break;
        case Probe::PROBE_TYPE_SH:
            if (auto &texture = probe->GetTexture()) {
                SetUniform(m_uniform_sh_map, texture.get());
                SetUniform(m_uniform_has_sh_map, 1);
            } else {
                SetUniform(m_uniform_has_sh_map, 0);
            }
            break;
        }
    }

    if (!env->GetProbeManager()->EnvMapEnabled()) {
        if (auto &cubemap = env->GetGlobalCubemap()) {
            cubemap->Prepare();

            SetUniform(m_uniform_env_global_cubemap, cubemap.get());
        }
    }
}

void DeferredRenderingShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);

    Matrix4 inverse_proj_view(camera->GetViewMatrix());
    inverse_proj_view *= camera->GetProjectionMatrix();
    inverse_proj_view.Invert();

    // TODO: move into general shader calculations
    SetUniform(m_uniform_inverse_view_proj_matrix, inverse_proj_view);
    SetUniform(m_uniform_camera_position, camera->GetTranslation());
}
} // namespace hyperion
