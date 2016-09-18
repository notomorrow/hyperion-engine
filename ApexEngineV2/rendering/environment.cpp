#include "environment.h"

namespace apex {
Environment *Environment::instance = nullptr;
const Vector2 Environment::possion_disk[16] = {
    Vector2(-0.94201624f, -0.39906216f),
    Vector2(0.94558609f, -0.76890725f),
    Vector2(-0.094184101f, -0.92938870f),
    Vector2(0.34495938f, 0.29387760f),
    Vector2(-0.91588581f, 0.45771432f),
    Vector2(-0.81544232f, -0.87912464f),
    Vector2(-0.38277543f, 0.27676845f),
    Vector2(0.97484398f, 0.75648379f),
    Vector2(0.44323325f, -0.97511554f),
    Vector2(0.53742981f, -0.47373420f),
    Vector2(-0.26496911f, -0.41893023f),
    Vector2(0.79197514f, 0.19090188f),
    Vector2(-0.24188840f, 0.99706507f),
    Vector2(-0.81409955f, 0.91437590f),
    Vector2(0.19984126f, 0.78641367f),
    Vector2(0.14383161f, -0.14100790f)
};

Environment *Environment::GetInstance()
{
    if (instance == nullptr) {
        instance = new Environment();
    }
    return instance;
}

Environment::Environment()
    : m_gravity(0.0f, -9.81f, 0.0f)
{
    shadows_enabled = false;
    num_cascades = 1;
    for (int i = 0; i < 4; i++) {
        shadow_maps[i] = nullptr;
        shadow_matrices[i] = Matrix4::Identity();
    }
}

const DirectionalLight &Environment::GetSun() const
{
    return sun;
}

DirectionalLight &Environment::GetSun()
{
    return sun;
}

bool Environment::ShadowsEnabled() const
{
    return shadows_enabled;
}

void Environment::SetShadowsEnabled(bool b)
{
    shadows_enabled = b;
}

int Environment::NumCascades() const
{
    return num_cascades;
}

void Environment::SetNumCascades(int i)
{
    num_cascades = i;
}

int Environment::GetShadowSplit(int i) const
{
    return shadow_splits[i];
}

void Environment::SetShadowSplit(int i, int split)
{
    shadow_splits[i] = split;
}

std::shared_ptr<Texture> Environment::GetShadowMap(int i) const
{
    return shadow_maps[i];
}

void Environment::SetShadowMap(int i, std::shared_ptr<Texture> tex)
{
    shadow_maps[i] = tex;
}

const Matrix4 &Environment::GetShadowMatrix(int i) const
{
    return shadow_matrices[i];
}

void Environment::SetShadowMatrix(int i, const Matrix4 &mat)
{
    shadow_matrices[i] = mat;
}
} // namespace apex