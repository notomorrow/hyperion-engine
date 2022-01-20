#ifndef SHADOW_MAPPING_H
#define SHADOW_MAPPING_H

#include "../environment.h"
#include "../texture.h"
#include "../framebuffer_2d.h"
#include "../../math/vector3.h"
#include "../../math/bounding_box.h"
#include "../../math/matrix_util.h"
#include "../camera/ortho_camera.h"

#include <array>

namespace apex {
class ShadowMapping {
public:
    ShadowMapping(Camera *view_cam, double max_dist, bool use_fbo = true);
    virtual ~ShadowMapping();

    const Vector3 &GetLightDirection() const;
    void SetLightDirection(const Vector3 &dir);

    inline const Vector3 &GetOrigin() const { return m_origin; }
    inline void SetOrigin(const Vector3 &origin) { m_origin = origin; }

    Camera *GetShadowCamera();

    std::shared_ptr<Texture> GetShadowMap();

    inline bool IsVarianceShadowMapping() const { return m_is_variance_shadow_mapping; }
    void SetVarianceShadowMapping(bool value);

    virtual void Begin();
    virtual void End();
    Camera *view_cam; // temp

protected:
    double max_dist;

    Camera *shadow_cam;
    Framebuffer *fbo;

    Vector3 maxes, mins, light_direction;
    std::array<Vector3, 8> frustum_corners_ls;
    std::array<Vector3, 8> frustum_corners_ws;
    BoundingBox bb;

    bool m_is_variance_shadow_mapping;

    Vector3 m_center_pos;
    Vector3 m_origin;

    bool m_use_fbo;

    void TransformPoints(const std::array<Vector3, 8> &in_vec,
        std::array<Vector3, 8> &out_vec, const Matrix4 &mat) const;
    void UpdateFrustumPoints(std::array<Vector3, 8> &points);
};
} // namespace apex

#endif
