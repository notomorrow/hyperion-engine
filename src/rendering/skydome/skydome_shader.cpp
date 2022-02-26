#include "skydome_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"
#include "../../math/math_util.h"

namespace hyperion {
SkydomeShader::SkydomeShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/skydome.vert");
    const std::string fs_path("shaders/skydome.frag");

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

    if (properties.HasValue("CLOUDS")) {
        has_clouds = properties.GetValue("CLOUDS").IsTruthy();
    } else {
        has_clouds = false;
    }

    if (has_clouds) {
        noise_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("textures/clouds.png");
        if (noise_map == nullptr) {
            throw std::runtime_error("Could not load noise map!");
        }
    }
    m_global_time = 0.0f;

    sun_color = Vector4(0.05f, 0.02f, 0.01f, 1.0f);

    m_uniform_noise_map = m_uniforms.Acquire("u_noiseMap").id;
    m_uniform_fg = m_uniforms.Acquire("fg").id;
    m_uniform_fg2 = m_uniforms.Acquire("fg2").id;
    m_uniform_sun_color = m_uniforms.Acquire("u_sunColor").id;
    m_uniform_global_time = m_uniforms.Acquire("u_globalTime").id;

    G = -0.990f;

    SetUniform(m_uniform_fg, G);
    SetUniform(m_uniform_fg2, G * G);
}

void SkydomeShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);

    auto *env = Environment::GetInstance();
    Shader::SetLightUniforms(env);

    if (has_clouds) {
        noise_map->Prepare();

        SetUniform(m_uniform_noise_map, noise_map.get());
    }

    SetUniform(m_uniform_global_time, m_global_time);
    SetUniform(m_uniform_sun_color, sun_color);
}

void SkydomeShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    // Sky dome should follow the camera
    Transform updated_transform(transform);
    updated_transform.SetTranslation(Vector3(
        camera->GetTranslation().x,
        camera->GetTranslation().y - 5.0,
        camera->GetTranslation().z
    ));

    Shader::ApplyTransforms(updated_transform, camera);
}

void SkydomeShader::SetGlobalTime(float global_time)
{
    m_global_time = global_time;
}
} // namespace hyperion
