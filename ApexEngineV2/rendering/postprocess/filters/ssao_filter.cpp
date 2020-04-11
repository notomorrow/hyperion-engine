#include "./ssao_filter.h"
#include "../../../asset/asset_manager.h"
#include "../../shader_manager.h"
#include "../../shaders/post/ssao.h"

namespace apex {

SSAOFilter::SSAOFilter()
    : PostFilter(ShaderManager::GetInstance()->GetShader<SSAOShader>(ShaderProperties {
        { "KERNEL_SIZE", 32.0f },
        { "CAP_MIN_DISTANCE", 0.00001f },
        { "CAP_MAX_DISTANCE", 0.01f },
        { "NO_GI", 1.0f }
      }))
{
    for (int i = 0; i < m_kernel.size(); i++) {
        m_kernel[i] = Vector3(MathUtil::Random(-0.1f, 0.1f), MathUtil::Random(-0.1f, 0.1f), MathUtil::Random(-0.1f, 0.1f));

        m_shader->SetUniform(std::string("u_kernel[") + std::to_string(i) + "]", m_kernel[i]);
    }

    m_noise_scale = Vector2(100.0f);
    m_shader->SetUniform("u_noiseScale", m_noise_scale);

    /*m_noise_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/noise.png");

    if (m_noise_map == nullptr) {
        throw std::runtime_error("Could not load noise map!");
    }*/
}

void SSAOFilter::SetUniforms(Camera *cam)
{
    /*Texture::ActiveTexture(5);
    m_noise_map->Use();
    m_shader->SetUniform("u_noiseMap", 5);*/

    m_shader->SetUniform("u_radius", 0.4f);

    m_shader->SetUniform("u_view", cam->GetViewMatrix());
    m_shader->SetUniform("u_projectionMatrix", cam->GetProjectionMatrix());

    Matrix4 inverse_projection(cam->GetProjectionMatrix());
    inverse_projection.Invert();

    m_shader->SetUniform("u_inverseProjectionMatrix", inverse_projection);
}

} // namespace apex
