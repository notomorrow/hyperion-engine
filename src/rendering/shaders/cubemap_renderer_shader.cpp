#include "cubemap_renderer_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"
#include "../../math/matrix_util.h"

namespace hyperion {
CubemapRendererShader::CubemapRendererShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/cubemap_renderer.vert");
    const std::string fs_path("shaders/cubemap_renderer.frag");
    const std::string gs_path("shaders/cubemap_renderer.geom");

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

    m_uniform_camera_position = m_uniforms.Acquire("u_camerapos").id;
    m_uniform_diffuse_color = m_uniforms.Acquire("u_diffuseColor").id;

    m_uniform_cube_matrices[0] = m_uniforms.Acquire("u_shadowMatrices[0]").id;
    m_uniform_cube_matrices[1] = m_uniforms.Acquire("u_shadowMatrices[1]").id;
    m_uniform_cube_matrices[2] = m_uniforms.Acquire("u_shadowMatrices[2]").id;
    m_uniform_cube_matrices[3] = m_uniforms.Acquire("u_shadowMatrices[3]").id;
    m_uniform_cube_matrices[4] = m_uniforms.Acquire("u_shadowMatrices[4]").id;
    m_uniform_cube_matrices[5] = m_uniforms.Acquire("u_shadowMatrices[5]").id;
}

void CubemapRendererShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    auto *env = Environment::GetInstance();
    Shader::SetLightUniforms(env);

    SetUniform(m_uniform_diffuse_color, mat.diffuse_color);
}

void CubemapRendererShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);

    SetUniform(m_uniform_camera_position, camera->GetTranslation());
}
} // namespace hyperion
