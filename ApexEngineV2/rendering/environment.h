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

    inline DirectionalLight &GetSun() { return m_sun; }
    inline const DirectionalLight &GetSun() const { return m_sun; }

    inline const Vector3 &GetGravity() const { return m_gravity; }
    inline void SetGravity(const Vector3 &gravity) { m_gravity = gravity; }

    inline bool ShadowsEnabled() const { return m_shadows_enabled; }
    inline void SetShadowsEnabled(bool shadows_enabled) { m_shadows_enabled = shadows_enabled; }
    inline int NumCascades() const { return m_num_cascades; }
    inline void SetNumCascades(int num_cascades) { m_num_cascades = num_cascades; }
    inline int GetShadowSplit(int i) const { return m_shadow_splits[i]; }
    inline void SetShadowSplit(int i, int split) { m_shadow_splits[i] = split; }
    inline std::shared_ptr<Texture> GetShadowMap(int i) const { return m_shadow_maps[i]; }
    inline void SetShadowMap(int i, const std::shared_ptr<Texture> &shadow_map) { m_shadow_maps[i] = shadow_map; }
    inline const Matrix4 &GetShadowMatrix(int i) const { return m_shadow_matrices[i]; }
    inline void SetShadowMatrix(int i, const Matrix4 &shadow_matrix) { m_shadow_matrices[i] = shadow_matrix; }

private:
    static Environment *instance;

    DirectionalLight m_sun;

    Vector3 m_gravity;

    bool m_shadows_enabled;
    int m_num_cascades;
    std::array<int, 4> m_shadow_splits;
    std::array<std::shared_ptr<Texture>, 4> m_shadow_maps;
    std::array<Matrix4, 4> m_shadow_matrices;
};
} // namespace apex

#endif