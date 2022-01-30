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

    SetUniform("C_albedo", mat.diffuse_color);

    for (auto it = mat.textures.begin(); it != mat.textures.end(); it++) {
        if (it->second == nullptr) {
            continue;
        }

        it->second->Prepare();

        SetUniform(it->first, it->second.get());
        SetUniform(std::string("Has") + it->first, 1);
    }
}

void GIVoxelShader::ApplyTransforms(const Transform &transform, Camera *camera)
{
    Shader::ApplyTransforms(transform, camera);
    SetUniform("CameraPosition", camera->GetTranslation());

    Transform world_to_unit_cube;
    world_to_unit_cube.SetTranslation(-1.0f);
    world_to_unit_cube.SetScale(2.0f);


    Vector3 voxel_bias(20.0f); // todo
    world_to_unit_cube *= Transform(Vector3(voxel_bias), Vector3(5.0f)/* scale*/, Quaternion());

    Matrix4 unit_cube_to_ndc = camera->GetViewProjectionMatrix();

    Matrix4 world_to_ndc = unit_cube_to_ndc * world_to_unit_cube.GetMatrix();

    Matrix4 ndc_to_tex = Matrix4(unit_cube_to_ndc).Transpose();

  //  SetUniform("u_projMatrix", world)

    SetUniform("mvp", world_to_ndc);

    SetUniform("StorageTransformMatrix", ndc_to_tex);
}
} // namespace hyperion
