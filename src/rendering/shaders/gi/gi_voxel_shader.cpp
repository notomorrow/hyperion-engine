#include "gi_voxel_shader.h"
#include "../../shader_manager.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../environment.h"

#include "../../../core_engine.h"

namespace hyperion {
GIVoxelShader::GIVoxelShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/gi/voxel.vert");
    const std::string fs_path("shaders/gi/voxel.frag");
    const std::string gs_path("shaders/gi/voxel.geom");

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

    if (ShaderManager::GetInstance()->GetBaseShaderProperties().GetValue("VCT_GEOMETRY_SHADER").IsTruthy()) {
        AddSubShader(
            Shader::SubShaderType::SUBSHADER_GEOMETRY,
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(gs_path)->GetText(),
            properties,
            gs_path
        );
    }

    m_uniform_albedo = m_uniforms.Acquire("C_albedo").id;
    m_uniform_emissiveness = m_uniforms.Acquire("Emissiveness").id;
    m_uniform_camera_position = m_uniforms.Acquire("CameraPosition").id;

    m_uniform_voxel_map = m_uniforms.Acquire("VoxelMap").id;
    m_uniform_voxel_scene_scale = m_uniforms.Acquire("VoxelSceneScale").id;
    m_uniform_voxel_probe_position = m_uniforms.Acquire("VoxelProbePosition").id;
}

void GIVoxelShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::CULL_FACE);

    SetUniform(m_uniform_albedo, mat.diffuse_color);
    SetUniform(m_uniform_emissiveness, mat.GetParameter(MATERIAL_PARAMETER_EMISSIVENESS)[0]);

    auto env = Environment::GetInstance();

    for (int i = 0; i < env->GetProbeManager()->NumProbes(); i++) {
        const auto &probe = env->GetProbeManager()->GetProbe(i);

        switch (probe->GetProbeType()) {
        case Probe::PROBE_TYPE_VCT:
            if (auto &texture = probe->GetTexture()) {
                SetUniform(m_uniform_voxel_probe_position, probe->GetOrigin());
                SetUniform(m_uniform_voxel_scene_scale, probe->GetBounds().GetDimensions());
                SetUniform(m_uniform_voxel_map, texture.get());
            }
            break;
        default:
            // do nothing
            break;
        }
    }
}

void GIVoxelShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);

    SetUniform(m_uniform_camera_position, camera->GetTranslation());
}
} // namespace hyperion
