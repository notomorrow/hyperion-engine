#include "./normals_shader.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

namespace apex {
NormalsShader::NormalsShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/normals.vert");
    const std::string fs_path("res/shaders/normals.frag");

    AddSubShader(SubShader(GL_VERTEX_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
            properties, vs_path)
        )
    );

    AddSubShader(SubShader(GL_FRAGMENT_SHADER,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
            properties, fs_path)
        )
    );
}

void NormalsShader::ApplyMaterial(const Material &mat)
{
}

void NormalsShader::ApplyTransforms(const Matrix4 &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("u_camerapos", camera->GetTranslation());
}
} // namespace apex
