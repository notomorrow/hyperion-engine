#include "particle_shader.h"
#include "../asset/asset_manager.h"
#include "../asset/text_loader.h"
#include "../util/shader_preprocessor.h"

namespace apex {
ParticleShader::ParticleShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/particle.vert");
    const std::string fs_path("res/shaders/particle.frag");

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
}

void ParticleShader::ApplyMaterial(const Material &mat)
{
    if (mat.texture0 != nullptr) {
        Texture::ActiveTexture(0);
        mat.texture0->Use();
        SetUniform("u_diffuseTexture", 0);
    }
}

void ParticleShader::ApplyTransforms(const Matrix4 &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
}
} // namespace apex