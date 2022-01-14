#include "shadow_mapping.h"
#include "../../core_engine.h"
#include "../../math/frustum.h"
#include "../../util.h"

namespace apex {
ShadowMapping::ShadowMapping(Camera *view_cam, double max_dist)
    : view_cam(view_cam), max_dist(max_dist)
{
    shadow_cam = new OrthoCamera(-1, 1, -1, 1, -1, 1);

    fbo = new Framebuffer2D(1024, 1024, true, true, false, false);
}

ShadowMapping::~ShadowMapping()
{
    delete fbo;
    delete shadow_cam;
}

const Vector3 &ShadowMapping::GetLightDirection() const
{
    return light_direction;
}

void ShadowMapping::SetLightDirection(const Vector3 &dir)
{
    light_direction = dir;
}

OrthoCamera *ShadowMapping::GetShadowCamera()
{
    return shadow_cam;
}

std::shared_ptr<Texture> ShadowMapping::GetShadowMap()
{
    return fbo->GetDepthTexture();
}

void ShadowMapping::Begin()
{
    UpdateFrustumPoints(frustum_corners_ws);
    Vector3 center_pos;
    for (size_t i = 0; i < frustum_corners_ws.size(); i++) {
        center_pos += frustum_corners_ws[i];
    }
    center_pos /= frustum_corners_ws.size();

    Matrix4 new_view, new_proj;
    MatrixUtil::ToLookAt(new_view, center_pos - light_direction, center_pos, Vector3::UnitY());

    TransformPoints(frustum_corners_ws, frustum_corners_ls, new_view);

    maxes = Vector3(std::numeric_limits<float>::min());
    mins = Vector3(std::numeric_limits<float>::max());

    for (size_t i = 0; i < frustum_corners_ls.size(); i++) {
        auto &corner = frustum_corners_ls[i];
        if (corner.x > maxes.x) {
            maxes.x = corner.x;
        } else if (corner.x < mins.x) {
            mins.x = corner.x;
        }
        if (corner.y > maxes.y) {
            maxes.y = corner.y;
        } else if (corner.y < mins.y) {
            mins.y = corner.y;
        }
        if (corner.z > maxes.z) {
            maxes.z = corner.z;
        } else if (corner.z < mins.z) {
            mins.z = corner.z;
        }
    }

    MatrixUtil::ToOrtho(new_proj, mins.x, maxes.x, mins.y, maxes.y, -max_dist, max_dist);

    shadow_cam->SetViewMatrix(new_view);
    shadow_cam->SetProjectionMatrix(new_proj);

    fbo->Use();

    glDepthMask(true);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    glCullFace(GL_FRONT);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);

    // glDepthMask(true);
    // glClearDepth(1.0);
    // glDepthFunc(GL_LESS);
    // glEnable(GL_DEPTH_TEST);
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glCullFace(GL_FRONT);
}

void ShadowMapping::End()
{
    glCullFace(GL_BACK);
    fbo->End();
}

void ShadowMapping::TransformPoints(const std::array<Vector3, 8> &in_vec,
    std::array<Vector3, 8> &out_vec, const Matrix4 &mat) const
{
    for (size_t i = 0; i < in_vec.size(); i++) {
        out_vec[i] = in_vec[i] * mat;
    }
}

void ShadowMapping::UpdateFrustumPoints(std::array<Vector3, 8> &points)
{
    bb = BoundingBox(Vector3::Round(view_cam->GetTranslation() - max_dist),
        Vector3::Round(view_cam->GetTranslation() + max_dist));

    points[0] = bb.GetMin();
    points[1] = bb.GetMax();
    points[2] = Vector3(points[0].x, points[0].y, points[1].z);
    points[3] = Vector3(points[0].x, points[1].y, points[0].z);
    points[4] = Vector3(points[1].x, points[0].y, points[0].z);
    points[5] = Vector3(points[0].x, points[1].y, points[1].z);
    points[6] = Vector3(points[1].x, points[0].y, points[1].z);
    points[7] = Vector3(points[1].x, points[1].y, points[0].z);
}
} // namespace apex
