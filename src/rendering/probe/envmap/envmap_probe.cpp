#include "envmap_probe.h"
#include "envmap_probe_camera.h"
#include "../../environment.h"
#include "../../shader_manager.h"
#include "../../shaders/cubemap_renderer_shader.h"
#include "../../renderer.h"

#include "../../camera/perspective_camera.h"
#include "../../../math/matrix_util.h"

#include "../../../core_engine.h"
#include "../../../opengl.h"
#include "../../../gl_util.h"

namespace hyperion {
EnvMapProbe::EnvMapProbe(const Vector3 &origin, const BoundingBox &bounds, int width, int height, float _near, float _far)
    : Probe(fbom::FBOMObjectType("ENVMAP_PROBE"), ProbeType::PROBE_TYPE_ENVMAP, origin, bounds),
      m_width(width),
      m_height(height),
      m_near(_near),
      m_far(_far),
      m_render_tick(0.0),
      m_render_index(0),
      m_is_first_run(true)
{
    m_fbo = new FramebufferCube(width, height);

    m_rendered_texture = GetColorTexture();

    m_cubemap_renderer_shader = ShaderManager::GetInstance()->GetShader<CubemapRendererShader>(ShaderProperties());

    for (int i = 0; i < m_cameras.size(); i++) {
        ProbeRegion region;
        region.origin = m_origin;
        region.bounds = m_bounds;
        region.direction = m_directions[i].first;
        region.up_vector = m_directions[i].second;
        region.index = i;

        auto *cam = new EnvMapProbeCamera(region, width, height, _near, _far);
        cam->SetTexture(m_rendered_texture.get());
        m_cameras[i] = cam;
    }
}

EnvMapProbe::~EnvMapProbe()
{
    for (ProbeCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        delete cam;
    }

    delete m_fbo;
}

void EnvMapProbe::Update(double dt)
{
    m_render_tick += dt;

    for (ProbeCamera *cam : m_cameras) {
        cam->Update(dt); // update matrices
    }
}

void EnvMapProbe::Render(Renderer *renderer, Camera *cam)
{
    if (!ProbeManager::GetInstance()->EnvMapEnabled()) {
        return;
    }

    if (m_is_first_run) {
        RenderCubemap(renderer, cam);

        m_is_first_run = false;

        return;
    }

    if (m_render_tick < 0.25) {
        return;
    }

    m_render_tick = 0.0;

    RenderCubemap(renderer, cam);
}


void EnvMapProbe::RenderCubemap(Renderer *renderer, Camera *cam)
{
    // TODO: better frustum culling of this
    SceneManager::GetInstance()->GetOctree()->UpdateVisibilityState(
        Octree::VisibilityState::CameraType::VIS_CAMERA_OTHER0,
        cam->GetFrustum()
    );

    m_fbo->Use();

    CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

    for (int i = 0; i < m_cameras.size(); i++) {
        m_cameras[i]->Render(renderer, cam);
        m_cubemap_renderer_shader->SetUniform(
            m_cubemap_renderer_shader->m_uniform_cube_matrices[i],
            m_cameras[i]->GetCamera()->GetViewProjectionMatrix()
        );
    }

    renderer->RenderBucket(
        cam,
        Spatial::Bucket::RB_SKY,
        Octree::VisibilityState::CameraType::VIS_CAMERA_OTHER0,
        m_cubemap_renderer_shader.get()
    );

    renderer->RenderBucket(
        cam,
        Spatial::Bucket::RB_TRANSPARENT,
        Octree::VisibilityState::CameraType::VIS_CAMERA_OTHER0,
        m_cubemap_renderer_shader.get()
    );

    renderer->RenderBucket(
        cam,
        Spatial::Bucket::RB_OPAQUE,
        Octree::VisibilityState::CameraType::VIS_CAMERA_OTHER0,
        m_cubemap_renderer_shader.get()
    );

    m_fbo->End();

    Environment::GetInstance()->SetGlobalCubemap(GetColorTexture());
}

std::shared_ptr<Renderable> EnvMapProbe::CloneImpl()
{
    return std::make_shared<EnvMapProbe>(m_origin, m_bounds, m_width, m_height, m_near, m_far);
}
} // namespace hyperion
