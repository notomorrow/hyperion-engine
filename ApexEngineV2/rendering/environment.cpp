#include "environment.h"

namespace apex {
Environment *Environment::instance = nullptr;

Environment *Environment::GetInstance()
{
    if (instance == nullptr) {
        instance = new Environment();
    }
    return instance;
}

Environment::Environment()
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
}