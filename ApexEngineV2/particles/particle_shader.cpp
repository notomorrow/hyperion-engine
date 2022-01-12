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

    AddSubShader(SubShader(Shader::SubShaderType::SUBSHADER_VERTEX,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(vs_path)->GetText(),
            properties, vs_path)
        ));

    AddSubShader(SubShader(Shader::SubShaderType::SUBSHADER_FRAGMENT,
        ShaderPreprocessor::ProcessShader(
            AssetManager::GetInstance()->LoadFromFile<TextLoader::LoadedText>(fs_path)->GetText(),
            properties, fs_path)
        ));
}

void ParticleShader::ApplyMaterial(const Material &mat)
{
    int texture_index = 1;

    for (auto it = mat.textures.begin(); it != mat.textures.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }

        Texture::ActiveTexture(texture_index);
        it->second->Use();
        SetUniform(it->first, texture_index);
        SetUniform(std::string("Has") + it->first, 1);

        texture_index++;
    }
}

void ParticleShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
}
} // namespace apex
