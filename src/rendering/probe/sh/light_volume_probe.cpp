#include "light_volume_probe.h"
#include "light_volume_probe_camera.h"
#include "light_volume_grid.h"
#include "../../environment.h"
#include "../../shader_manager.h"
#include "../../shaders/cubemap_renderer_shader.h"
#include "../../shaders/compute/sh_compute_shader.h"
#include "../../renderer.h"

#include "../../camera/perspective_camera.h"
#include "../../../math/matrix_util.h"

#include "../../../core_engine.h"
#include "../../../opengl.h"
#include "../../../gl_util.h"

namespace hyperion {
LightVolumeProbe::LightVolumeProbe(const Vector3 &origin, const BoundingBox &bounds, const Vector3 &grid_offset)
    : Probe(fbom::FBOMObjectType("LIGHT_VOLUME_PROBE"), origin, bounds),
      m_grid_offset(grid_offset)
{
    for (int i = 0; i < m_cameras.size(); i++) {
        ProbeRegion region;
        region.origin = m_origin;
        region.bounds = m_bounds;
        region.direction = m_directions[i].first;
        region.up_vector = m_directions[i].second;
        region.index = i;

        m_cameras[i] = new LightVolumeProbeCamera(region, LightVolumeGrid::cubemap_width, LightVolumeGrid::cubemap_width, 0.01f, 10.0f);
    }
}

LightVolumeProbe::~LightVolumeProbe()
{
    for (ProbeCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        delete cam;
    }
}

void LightVolumeProbe::Bind(Shader *shader)
{
    // TODO: Index
    //GetCubemap()->Prepare();
    //shader->SetUniform("env_GlobalCubemap", GetCubemap().get());

    shader->SetUniform("EnvProbe.position", m_origin);
    shader->SetUniform("EnvProbe.max", m_bounds.GetMax());
    shader->SetUniform("EnvProbe.min", m_bounds.GetMin());
}

void LightVolumeProbe::Update(double dt)
{
    for (ProbeCamera *cam : m_cameras) {
        cam->Update(dt); // update matrices
    }
}

void LightVolumeProbe::Render(Renderer *renderer, Camera *cam)
{
    for (int i = 0; i < m_cameras.size(); i++) {
        m_cameras[i]->Render(renderer, cam);
        m_shader->SetUniform("u_shadowMatrices[" + std::to_string(i) + "]", m_cameras[i]->GetCamera()->GetViewProjectionMatrix());
    }

    m_shader->SetUniform("LightVolumeGridOffset", m_grid_offset);

    renderer->RenderBucket(
        cam,
        renderer->GetBucket(Renderable::RB_SKY),
        m_shader.get(),
        false
    );

    renderer->RenderBucket(
        cam,
        renderer->GetBucket(Renderable::RB_TRANSPARENT),
        m_shader.get(),
        false
    );

    renderer->RenderBucket(
        cam,
        renderer->GetBucket(Renderable::RB_OPAQUE),
        m_shader.get(),
        false
    );
}

std::shared_ptr<Renderable> LightVolumeProbe::CloneImpl()
{
    return std::make_shared<LightVolumeProbe>(m_origin, m_bounds, m_grid_offset);
}
} // namespace hyperion
