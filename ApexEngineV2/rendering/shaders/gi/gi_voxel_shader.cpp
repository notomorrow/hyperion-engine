#include "gi_voxel_shader.h"
#include "../../../asset/asset_manager.h"
#include "../../../asset/text_loader.h"

namespace hyperion {
GIVoxelShader::GIVoxelShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/gi/voxel.vert");
    const std::string fs_path("res/shaders/gi/voxel.frag");
    const std::string gs_path("res/shaders/gi/voxel.geom");

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
}

void GIVoxelShader::ApplyMaterial(const Material &mat)
{
    Shader::ApplyMaterial(mat);
}

void GIVoxelShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("CameraPosition", camera->GetTranslation());
}
} // namespace hyperion
