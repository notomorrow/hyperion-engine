#ifndef PSSM_SHADOW_MAPPING_H
#define PSSM_SHADOW_MAPPING_H

#include "shadow_mapping.h"
#include "../renderer.h"
#include "../camera/camera.h"

#include <vector>
#include <memory>

namespace hyperion {

class Shader;

class PssmShadowMapping {
public:
    PssmShadowMapping(Camera *view_cam, int num_splits, double max_dist);
    ~PssmShadowMapping() = default;

    int NumSplits() const;
    void SetLightDirection(const Vector3 &dir);

    inline const Vector3 &GetOrigin() const { return m_origin; }
    void SetOrigin(const Vector3 &origin);

    inline bool IsVarianceShadowMapping() const { return m_is_variance_shadow_mapping; }
    void SetVarianceShadowMapping(bool value);

    void Render(Renderer *renderer);

private:
    const int num_splits;
    bool m_is_variance_shadow_mapping;
    std::shared_ptr<Shader> m_depth_shader;
    std::vector<std::shared_ptr<ShadowMapping>> shadow_renderers;
    Vector3 m_origin;
};

} // namespace hyperion

#endif
