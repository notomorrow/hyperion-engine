#include "skydome_shader.h"
#include "../environment.h"
#include "../../asset/asset_manager.h"
#include "../../asset/text_loader.h"
#include "../../util/shader_preprocessor.h"
#include "../../math/math_util.h"

namespace apex {
SkydomeShader::SkydomeShader(const ShaderProperties &properties)
    : Shader(properties)
{
    const std::string vs_path("res/shaders/skydome.vert");
    const std::string fs_path("res/shaders/skydome.frag");

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

    if (properties.find("CLOUDS") != properties.end()) {
        has_clouds = bool(properties.at("CLOUDS"));
    } else {
        has_clouds = false;
    }

    if (has_clouds) {
        noise_map = AssetManager::GetInstance()->LoadFromFile<Texture2D>("res/textures/clouds.png");
        if (noise_map == nullptr) {
            throw std::runtime_error("Could not load noise map!");
        }
    }
    m_global_time = 0.0f;

    sun_color = Vector4(0.05f, 0.02f, 0.01f, 1.0f);

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
    Kr4PI = Kr * 4.0f * MathUtil::PI;
    Km4PI = Km * 4.0f * MathUtil::PI;
    exposure = 2.0f;
    G = -0.990f;
    inner_radius = 100.0f;
    scale = 1.0f / (inner_radius * 1.025f - inner_radius);
    scale_depth = 0.25f;
    scale_over_scale_depth = scale / scale_depth;

    // Set uniform constants
    SetUniform("fKrESun", KrESun);
    SetUniform("fKmESun", KmESun);
    SetUniform("fOuterRadius", inner_radius * 1.025f);
    SetUniform("fInnerRadius", inner_radius);
    SetUniform("fOuterRadius2", pow(inner_radius * 1.025f, 2.0f));
    SetUniform("fInnerRadius2", inner_radius * inner_radius);
    SetUniform("fKr4PI", Kr4PI);
    SetUniform("fKm4PI", Km4PI);
    SetUniform("fg", G);
    SetUniform("fg2", G * G);
    SetUniform("fExposure", exposure);
}

void SkydomeShader::ApplyMaterial(const Material &mat)
{
    auto *env = Environment::GetInstance();

    if (has_clouds) {
        Texture::ActiveTexture(0);
        noise_map->Use();
        SetUniform("u_noiseMap", 0);
    }

    SetUniform("u_globalTime", m_global_time);
    SetUniform("v3LightPos", env->GetSun().GetDirection());
    SetUniform("u_sunColor", sun_color);

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

void SkydomeShader::ApplyTransforms(const Matrix4 &transform, Camera *camera)
{
    // Sky dome should follow the camera
    Matrix4 dome_model_mat = transform;
    dome_model_mat(0, 3) = camera->GetTranslation().x;
    dome_model_mat(1, 3) = camera->GetTranslation().y;
    dome_model_mat(2, 3) = camera->GetTranslation().z;

    Shader::ApplyTransforms(dome_model_mat, camera);

    SetUniform("v3CameraPos", camera->GetTranslation());
}

void SkydomeShader::SetGlobalTime(float global_time)
{
    m_global_time = global_time;
}
} // namespace apex