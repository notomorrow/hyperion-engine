#include "shadow_mapping.h"
#include "../../core_engine.h"
#include "../../math/frustum.h"
#include "../../scene/spatial.h"
#include "../shaders/depth_shader.h"
#include "../shader_manager.h"
#include "../environment.h"
#include "../renderer.h"
#include "../../util.h"

namespace hyperion {
ShadowMapping::ShadowMapping(double max_dist, int level, bool use_fbo)
    : Renderable(fbom::FBOMObjectType("SHADOW_MAPPING")),
      m_max_dist(max_dist),
      m_level(level),
      m_use_fbo(use_fbo),
      m_is_variance_shadow_mapping(ShaderManager::GetInstance()->GetBaseShaderProperties().GetValue("SHADOWS_VARIANCE").IsTruthy())
{
    shadow_cam = new OrthoCamera(-1, 1, -1, 1, -1, 1);

    m_depth_shader = ShaderManager::GetInstance()->GetShader<DepthShader>(ShaderProperties());

    fbo = new Framebuffer2D(2048, 2048, true, true, false, false);

    Environment::GetInstance()->SetShadowSplit(m_level, m_max_dist);
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
    light_direction = Vector3(dir).Normalize();
}

Camera *ShadowMapping::GetShadowCamera()
{
    return shadow_cam;
}

std::shared_ptr<Texture> ShadowMapping::GetShadowMap()
{
    return fbo->GetAttachment(Framebuffer::FramebufferAttachment::FRAMEBUFFER_ATTACHMENT_COLOR);
}

void ShadowMapping::Render(Renderer *renderer, Camera *cam)
{
    if (!renderer->GetEnvironment()->ShadowsEnabled()) {
        return;
    }

    UpdateFrustumPoints(frustum_corners_ws);

    Vector3 frustum_min(MathUtil::MaxSafeValue<float>());
    Vector3 frustum_max(MathUtil::MinSafeValue<float>());
    
    // m_center_pos = Vector3();

    for (size_t i = 0; i < frustum_corners_ws.size(); i++) {
        // m_center_pos += frustum_corners_ws[i];
        frustum_min = Vector3::Min(frustum_min, frustum_corners_ws[i]);
        frustum_max = Vector3::Max(frustum_max, frustum_corners_ws[i]);
    }

    //m_center_pos /= frustum_corners_ws.size();
    m_center_pos = (frustum_min + frustum_max) / 2;

    Matrix4 new_view, new_proj;
    MatrixUtil::ToLookAt(new_view, m_center_pos - light_direction, m_center_pos, Vector3::UnitY());

    TransformPoints(frustum_corners_ws, frustum_corners_ls, new_view);

    maxes = Vector3(MathUtil::MinSafeValue<float>());
    mins = Vector3(MathUtil::MaxSafeValue<float>());

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

    MatrixUtil::ToOrtho(new_proj, mins.x, maxes.x, mins.y, maxes.y, mins.z, maxes.z);
    //MatrixUtil::ToOrtho(new_proj, mins.x, maxes.x, mins.y, maxes.y, -100.0f, 100.0f);

    shadow_cam->SetViewMatrix(new_view);
    shadow_cam->SetProjectionMatrix(new_proj);

    if (m_use_fbo) {
        fbo->Use();
    }

    CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

    renderer->RenderBucket(
        GetShadowCamera(),
        renderer->GetBucket(Spatial::Bucket::RB_OPAQUE),
        m_depth_shader.get(),
        false
    );

    renderer->RenderBucket(
        GetShadowCamera(),
        renderer->GetBucket(Spatial::Bucket::RB_TRANSPARENT),
        m_depth_shader.get(),
        false
    );

    if (m_use_fbo) {
        fbo->End();
    }

    Environment::GetInstance()->SetShadowMap(m_level, GetShadowMap());
    Environment::GetInstance()->SetShadowMatrix(m_level, GetShadowCamera()->GetViewProjectionMatrix());
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
    bb = BoundingBox(
        Vector3::Round(m_origin - m_max_dist),
        Vector3::Round(m_origin + m_max_dist)
    );

    //points = bb.GetCorners();

    points[0] = bb.GetMin();
    points[1] = bb.GetMax();
    points[2] = Vector3(points[0].x, points[0].y, points[1].z);
    points[3] = Vector3(points[0].x, points[1].y, points[0].z);
    points[4] = Vector3(points[1].x, points[0].y, points[0].z);
    points[5] = Vector3(points[0].x, points[1].y, points[1].z);
    points[6] = Vector3(points[1].x, points[0].y, points[1].z);
    points[7] = Vector3(points[1].x, points[1].y, points[0].z);
}

void ShadowMapping::SetVarianceShadowMapping(bool value)
{
    if (value == m_is_variance_shadow_mapping) {
        return;
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("SHADOWS_VARIANCE", value)
    );

    if (auto color_texture = fbo->GetAttachment(Framebuffer::FramebufferAttachment::FRAMEBUFFER_ATTACHMENT_COLOR)) {
        if (value) {
            color_texture->SetFilter(CoreEngine::GLEnums::LINEAR, CoreEngine::GLEnums::LINEAR);
        } else {
            color_texture->SetFilter(CoreEngine::GLEnums::NEAREST, CoreEngine::GLEnums::NEAREST);
        }
    }

    m_is_variance_shadow_mapping = value;
}

std::shared_ptr<Renderable> ShadowMapping::CloneImpl()
{
    return std::make_shared<ShadowMapping>(m_max_dist, m_level, m_use_fbo);
}
} // namespace hyperion
