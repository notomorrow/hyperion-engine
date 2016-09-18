#include "clouds_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace apex {
CloudsShader::CloudsShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/clouds.vert");
    const std::string fs_path("res/shaders/clouds.frag");

    AddSubShader(SubShader(GL_VERTEX_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
            properties, vs_path)
        ));

    AddSubShader(SubShader(GL_FRAGMENT_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
            properties, fs_path)
        ));


    cloud_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("res\\textures\\clouds2.png");
    if (cloud_map == nullptr) {
        throw std::runtime_error("Could not load cloud map!");
    }

    m_global_time = 0.0f;
    m_cloud_color = Vector4(1.0);
}

void CloudsShader::ApplyMaterial(const Material &mat)
{
    Texture::ActiveTexture(0);
    cloud_map->Use();
    SetUniform("m_CloudMap", 0);

    SetUniform("m_GlobalTime", m_global_time);
    SetUniform("m_CloudColor", m_cloud_color);

    if (mat.alpha_blended) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    if (!mat.depth_test) {
        glDisable(GL_DEPTH_TEST);
    }
    if (!mat.depth_write) {
        glDepthMask(false);
    }
}

void CloudsShader::ApplyTransforms(const Matrix4 &transform, Camera *camera)
{
    // Cloud layer should follow the camera
    Matrix4 clouds_model_mat = transform;
    clouds_model_mat(0, 3) = camera->GetTranslation().x;
    clouds_model_mat(1, 3) = camera->GetTranslation().y + 6.0f;
    clouds_model_mat(2, 3) = camera->GetTranslation().z;

    Shader::ApplyTransforms(clouds_model_mat, camera);
}

void CloudsShader::SetCloudColor(const Vector4 &cloud_color)
{
    m_cloud_color = cloud_color;
}

void CloudsShader::SetGlobalTime(float global_time)
{
    m_global_time = global_time;
}
} // namespace apex