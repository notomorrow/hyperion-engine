#include "ssao.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"
#include "../../../util/shader_preprocessor.h"
#include "../../environment.h"

namespace hyperion {
SSAOShader::SSAOShader(const ShaderProperties &properties)
    : PostShader(properties)
{
    const std::string fs_path("shaders/filters/ssao.frag");

    AddSubShader(
        Shader::SubShaderType::SUBSHADER_FRAGMENT,
        AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
        properties,
        fs_path
    );

    m_uniform_inverse_view_proj = m_uniforms.Acquire("InverseViewProjMatrix").id;
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
}

void SSAOShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);

    Matrix4 inverse_proj_view(camera->GetViewMatrix());
    inverse_proj_view *= camera->GetProjectionMatrix();
    inverse_proj_view.Invert();

    // TODO: move into general shader calculations
    SetUniform(m_uniform_inverse_view_proj, inverse_proj_view);
}
} // namespace hyperion
