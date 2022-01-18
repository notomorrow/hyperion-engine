#include "cubemap_renderer_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"
#include "../../math/matrix_util.h"

namespace apex {
CubemapRendererShader::CubemapRendererShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/cubemap_renderer.vert");
    const std::string fs_path("res/shaders/cubemap_renderer.frag");
    const std::string gs_path("res/shaders/cubemap_renderer.geom");

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

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_GEOMETRY,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(gs_path)->GetText(),
        properties,
        gs_path
    );
}

void CubemapRendererShader::ApplyMaterial(const Material &mat)
{
}

void CubemapRendererShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("u_camerapos", camera->GetTranslation());


    Vector3 light_pos(0, 0, 0); //temp

    float near = 1.0f;
    float far = 25.0f;
    const float shadow_width = 256; // temp
    const float shadow_height = 256; // temp

    Matrix4 shadow_proj;
    MatrixUtil::ToPerspective(shadow_proj, 90.0f, shadow_width, shadow_height, near, far);

    MatrixUtil::ToLookAt(m_shadow_matrices[0], light_pos, light_pos + Vector3(1, 0, 0), Vector3(0, -1, 0));
    m_shadow_matrices[0] = shadow_proj * m_shadow_matrices[0];

    MatrixUtil::ToLookAt(m_shadow_matrices[1], light_pos, light_pos + Vector3(-1, 0, 0), Vector3(0, -1, 0));
    m_shadow_matrices[1] = shadow_proj * m_shadow_matrices[1];

    MatrixUtil::ToLookAt(m_shadow_matrices[2], light_pos, light_pos + Vector3(0, 1, 0), Vector3(0, 0, 1));
    m_shadow_matrices[2] = shadow_proj * m_shadow_matrices[2];

    MatrixUtil::ToLookAt(m_shadow_matrices[3], light_pos, light_pos + Vector3(0, -1, 0), Vector3(0, 0, -1));
    m_shadow_matrices[3] = shadow_proj * m_shadow_matrices[3];

    MatrixUtil::ToLookAt(m_shadow_matrices[4], light_pos, light_pos + Vector3(0, 0, 1), Vector3(0, -1, 0));
    m_shadow_matrices[4] = shadow_proj * m_shadow_matrices[4];

    MatrixUtil::ToLookAt(m_shadow_matrices[5], light_pos, light_pos + Vector3(0, 0, -1), Vector3(0, -1, 0));
    m_shadow_matrices[5] = shadow_proj * m_shadow_matrices[5];

    SetUniform("u_lightPos", light_pos);
    SetUniform("u_far", far);

    for (int i = 0; i < m_shadow_matrices.size(); i++) {
        SetUniform("u_shadowMatrices[" + std::to_string(i) + "]", m_shadow_matrices[i]);
    }
}
} // namespace apex
