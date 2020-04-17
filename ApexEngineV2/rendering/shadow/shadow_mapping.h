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
    ShadowMapping(Camera *view_cam, double max_dist);
    ~ShadowMapping();

    const Vector3 &GetLightDirection() const;
    void SetLightDirection(const Vector3 &dir);
    OrthoCamera *GetShadowCamera();
    std::shared_ptr<Texture> GetShadowMap();

    void Begin();
    void End();

private:
    double max_dist;

    OrthoCamera *shadow_cam;
    Camera *view_cam;
    Framebuffer *fbo;

    Vector3 maxes, mins, light_direction;
    std::array<Vector3, 8> frustum_corners_ls;
    std::array<Vector3, 8> frustum_corners_ws;
    BoundingBox bb;

    void TransformPoints(const std::array<Vector3, 8> &in_vec,
        std::array<Vector3, 8> &out_vec, const Matrix4 &mat) const;
    void UpdateFrustumPoints(std::array<Vector3, 8> &points);
};
} // namespace apex

#endif
