#include "skybox_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace hyperion {
SkyboxShader::SkyboxShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("shaders/skybox.vert");
    const std::string fs_path("shaders/skybox.frag");

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

    m_uniform_camera_position = m_uniforms.Acquire("u_camerapos").id;
}

void SkyboxShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);
}

void SkyboxShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Transform updated_transform(transform);
    updated_transform.SetTranslation(camera->GetTranslation());

    Shader::ApplyTransforms(updated_transform, camera);

    SetUniform(m_uniform_camera_position, camera->GetTranslation());
}
} // namespace hyperion
