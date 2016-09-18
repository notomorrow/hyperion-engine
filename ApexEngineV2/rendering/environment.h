#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "../math/matrix4.h"
#include "../math/vector2.h"
#include "../rendering/texture.h"
#include "lights/directional_light.h"

#include <memory>
#include <array>

namespace apex {
class Environment {
public:
    static Environment *GetInstance();
    static const Vector2 possion_disk[16];

    Environment();

    const DirectionalLight &GetSun() const;
    DirectionalLight &GetSun();

    inline const Vector3 &GetGravity() const { return m_gravity; }
    inline void SetGravity(const Vector3 &gravity) { m_gravity = gravity; }

    bool ShadowsEnabled() const;
    void SetShadowsEnabled(bool);
    int NumCascades() const;
    void SetNumCascades(int);
    int GetShadowSplit(int i) const;
    void SetShadowSplit(int i, int split);
    std::shared_ptr<Texture> GetShadowMap(int i) const;
    void SetShadowMap(int i, std::shared_ptr<Texture>);
    const Matrix4 &GetShadowMatrix(int i) const;
    void SetShadowMatrix(int i, const Matrix4 &);

private:
    static Environment *instance;

    DirectionalLight sun;

    Vector3 m_gravity;

    bool shadows_enabled;
    int num_cascades;
    std::array<int, 4> shadow_splits;
    std::array<std::shared_ptr<Texture>, 4> shadow_maps;
    std::array<Matrix4, 4> shadow_matrices;
};
} // namespace apex

#endif