#include "skydome_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"

#define _USE_MATH_DEFINES
#include <math.h>

namespace apex {
SkydomeShader::SkydomeShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/skydome.vert");
    const std::string fs_path("res/shaders/skydome.frag");

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

    if (properties.find("CLOUDS") != properties.end()) {
        has_clouds = bool(properties.at("CLOUDS"));
    } else {
        has_clouds = false;
    }

    if (has_clouds) {
        noise_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("res\\textures\\clouds.png");
        if (noise_map == nullptr) {
            throw std::runtime_error("Could not load noise map!");
        }
    }

    _camera_pos = Vector3::Zero();
    _camera_height = 0;
    _global_time = 0.0f;

    sun_color = Vector4(0.05, 0.02, 0.01, 1.0);

    wavelength = Vector3(0.731f, 0.612f, 0.455f);
    inv_wavelength4.x = 1.0f / pow(wavelength.x, 4.0f);
    inv_wavelength4.y = 1.0f / pow(wavelength.y, 4.0f);
    inv_wavelength4.z = 1.0f / pow(wavelength.z, 4.0f);

    num_samples = 4;
    Kr = 0.0025f;
    Km = 0.0015f;
    ESun = 100.0f;
    KrESun = Kr * ESun;
    KmESun = Km * ESun;
    Kr4PI = Kr * 4.0f * M_PI;
    Km4PI = Km * 4.0f * M_PI;
    exposure = 2.0f;
    G = -0.990f;
    inner_radius = 100.0f;
    scale = 1.0f / (inner_radius * 1.025f - inner_radius);
    scale_depth = 0.25f;
    scale_over_scale_depth = scale / scale_depth;
}

void SkydomeShader::ApplyMaterial(const Material &mat)
{
    auto *env = Environment::GetInstance();

    if (has_clouds) {
        CoreEngine::GetInstance()->ActiveTexture(0);
        SetUniform("u_noiseMap", 0);
        noise_map->Use();
    }

    SetUniform("v3CameraPos", _camera_pos);
    SetUniform("fCameraHeight2", float(_camera_height * _camera_height));
    SetUniform("u_globalTime", _global_time);

    SetUniform("v3LightPos", env->GetSun().GetDirection());
    SetUniform("v3InvWavelength", inv_wavelength4);
    SetUniform("fKrESun", KrESun);
    SetUniform("fKmESun", KmESun);
    SetUniform("fOuterRadius", inner_radius * 1.025f);
    SetUniform("fInnerRadius", inner_radius);
    SetUniform("fOuterRadius2", pow(inner_radius * 1.025f, 2.0f));
    SetUniform("fInnerRadius2", inner_radius * inner_radius);
    SetUniform("fKr4PI", Kr4PI);
    SetUniform("fKm4PI", Km4PI);
    SetUniform("fScale", scale);
    SetUniform("fScaleDepth", scale_depth);
    SetUniform("fScaleOverScaleDepth", scale_over_scale_depth);
    SetUniform("fSamples", (float)num_samples);
    SetUniform("nSamples", num_samples);
    SetUniform("fg", G);
    SetUniform("fg2", G * G);
    SetUniform("fExposure", exposure);
    SetUniform("u_sunColor", sun_color);

    if (mat.HasParameter("BlendMode") && mat.GetParameter("BlendMode")[0] == 1) {
        CoreEngine::GetInstance()->Enable(CoreEngine::BLEND); 
        CoreEngine::GetInstance()->BlendFunc(CoreEngine::SRC_ALPHA, CoreEngine::ONE_MINUS_SRC_ALPHA);
    }
}

void SkydomeShader::ApplyTransforms(const Matrix4 &model, const Matrix4 &view, const Matrix4 &proj)
{
    Shader::ApplyTransforms(model, view, proj);
}

void SkydomeShader::SetCameraPosition(const Vector3 &camera_pos)
{
    _camera_pos = camera_pos;
}

void SkydomeShader::SetCameraHeight(int camera_height)
{
    _camera_height = camera_height;
}

void SkydomeShader::SetGlobalTime(float global_time)
{
    _global_time = global_time;
}
}