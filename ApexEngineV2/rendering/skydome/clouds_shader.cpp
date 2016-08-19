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

    AddSubShader(SubShader(CoreEngine::VERTEX_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
            properties, vs_path)
        ));

    AddSubShader(SubShader(CoreEngine::FRAGMENT_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
            properties, fs_path)
        ));


    cloud_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("res\\textures\\clouds2.png");
    if (cloud_map == nullptr) {
        throw std::runtime_error("Could not load cloud map!");
    }

    _global_time = 0.0f;
    _cloud_color = Vector4(1.0);
}

void CloudsShader::ApplyMaterial(const Material &mat)
{
    Texture::ActiveTexture(0);
    cloud_map->Use();
    SetUniform("m_CloudMap", 0);

    SetUniform("m_GlobalTime", _global_time);
    SetUniform("m_CloudColor", _cloud_color);

    if (mat.alpha_blended) {
        CoreEngine::GetInstance()->Enable(CoreEngine::BLEND);
        CoreEngine::GetInstance()->BlendFunc(CoreEngine::SRC_ALPHA, CoreEngine::ONE_MINUS_SRC_ALPHA);
    }
    if (!mat.depth_test) {
        CoreEngine::GetInstance()->Disable(CoreEngine::DEPTH_TEST);
    }
    if (!mat.depth_write) {
        CoreEngine::GetInstance()->DepthMask(false);
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
    _cloud_color = cloud_color;
}

void CloudsShader::SetGlobalTime(float global_time)
{
    _global_time = global_time;
}
}