#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include "../math/matrix4.h"
#include "../math/vector2.h"
#include "../rendering/probe/probe_manager.h"
#include "../rendering/texture.h"
#include "../rendering/cubemap.h"
#include "./lights/directional_light.h"
#include "./lights/point_light.h"

#include <memory>
#include <array>
#include <vector>

namespace hyperion {
class Environment {
public:
    static Environment *GetInstance();
    static const Vector2 possion_disk[16];
    // for deferred rendering - the maximum number of pointlights to be sent to our
    // deferred renderer. they will be chosen based on on-screen visibilty, then sorted via
    // distance from the camera after that.
    static const size_t max_point_lights_on_screen;

    struct PointLightCameraData {
        PointLight *point_light;
        float distance;
    };

    Environment();
    Environment(const Environment &other) = delete;
    Environment &operator=(const Environment &other) = delete;
    ~Environment();

    inline DirectionalLight &GetSun() { return m_sun; }
    inline const DirectionalLight &GetSun() const { return m_sun; }

    inline const Vector3 &GetGravity() const { return m_gravity; }
    inline void SetGravity(const Vector3 &gravity) { m_gravity = gravity; }

    inline bool ShadowsEnabled() const { return m_shadows_enabled; }
    void SetShadowsEnabled(bool shadows_enabled);
    inline bool PSSMEnabled() const { return m_pssm_enabled; }
    void SetPSSMEnabled(bool pssm_enabled);
    inline int NumCascades() const { return m_num_cascades; }
    void SetNumCascades(int num_cascades);
    inline double GetShadowSplit(int i) const { return m_shadow_splits[i]; }
    inline void SetShadowSplit(int i, double split) { m_shadow_splits[i] = split; }
    inline std::shared_ptr<Texture> GetShadowMap(int i) const { return m_shadow_maps[i]; }
    inline void SetShadowMap(int i, const std::shared_ptr<Texture> &shadow_map) { m_shadow_maps[i] = shadow_map; }
    inline const Matrix4 &GetShadowMatrix(int i) const { return m_shadow_matrices[i]; }
    inline void SetShadowMatrix(int i, const Matrix4 &shadow_matrix) { m_shadow_matrices[i] = shadow_matrix; }

    inline size_t NumPointLights() const { return m_point_lights.size(); }
    inline size_t NumVisiblePointLights() const { return MathUtil::Min(m_point_lights_sorted.size(), max_point_lights_on_screen); }
    inline std::shared_ptr<PointLight> &GetPointLight(size_t index) { return m_point_lights[index]; }
    inline const std::shared_ptr<PointLight> &GetPointLight(size_t index) const { return m_point_lights[index]; }
    inline std::vector<PointLightCameraData> &GetVisiblePointLights() { return m_point_lights_sorted; }
    inline const std::vector<PointLightCameraData> &GetVisiblePointLights() const { return m_point_lights_sorted; }
    inline void AddPointLight(const std::shared_ptr<PointLight> &point_light) { m_point_lights.push_back(point_light); }
    // Todo: refactor into some sort of Control class that does this automatically
    void CollectVisiblePointLights(Camera *camera);
    void BindLights(Shader *shader) const;

    inline const std::shared_ptr<Cubemap> &GetGlobalCubemap() const { return m_global_cubemap; }
    inline std::shared_ptr<Cubemap> &GetGlobalCubemap() { return m_global_cubemap; }
    inline void SetGlobalCubemap(const std::shared_ptr<Cubemap> &cubemap) { m_global_cubemap = cubemap; }

    inline ProbeManager *GetProbeManager() { return m_probe_manager; }
    inline const ProbeManager *GetProbeManager() const { return m_probe_manager; }

private:
    static Environment *instance;

    DirectionalLight m_sun;
    std::vector<std::shared_ptr<PointLight>> m_point_lights;

    std::vector<PointLightCameraData> m_point_lights_sorted;

    std::shared_ptr<Cubemap> m_global_cubemap;

    Vector3 m_gravity;

    bool m_shadows_enabled;
    bool m_pssm_enabled;
    int m_num_cascades;
    std::array<double, 4> m_shadow_splits;
    std::array<std::shared_ptr<Texture>, 4> m_shadow_maps;
    std::array<Matrix4, 4> m_shadow_matrices;

    ProbeManager *m_probe_manager;
};
} // namespace hyperion

#endif
