#include "gi_mapper.h"

#include "../shader_manager.h"
#include "../shaders/gi/gi_voxel_shader.h"
#include "../shaders/gi/gi_voxel_clear_shader.h"
#include "../renderer.h"
#include "../environment.h"
#include "../../math/math_util.h"
#include "../../opengl.h"
#include "../../gl_util.h"

namespace hyperion {
GIMapper::GIMapper(const BoundingBox &bounds)
    : Renderable(RB_BUFFER),
      m_bounds(bounds),
      m_render_tick(0.0),
      m_render_index(0),
      m_is_first_run(true),
      m_directions({
          std::make_pair(Vector3(1, 0, 0), Vector3(0, -1, 0)),
          std::make_pair(Vector3(-1, 0, 0), Vector3(0, -1, 0)),
          std::make_pair(Vector3(0, 1, 0), Vector3(0, 0, 1)),
          std::make_pair(Vector3(0, -1, 0), Vector3(0, 0, -1)),
          std::make_pair(Vector3(0, 0, 1), Vector3(0, -1, 0)),
          std::make_pair(Vector3(0, 0, -1), Vector3(0, -1, 0))
      })
{
    SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelShader>(ShaderProperties()));

    for (int i = 0; i < m_cameras.size(); i++) {
        GIMapperRegion region;
        region.bounds = m_bounds;
        region.direction = m_directions[i].first;
        region.up_vector = m_directions[i].second;

        m_cameras[i] = new GIMapperCamera(region);
        m_cameras[i]->SetShader(m_shader);
    }
}

GIMapper::~GIMapper()
{
    for (GIMapperCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        delete cam;
    }
}

void GIMapper::SetOrigin(const Vector3 &origin)
{
    m_bounds.SetCenter(origin);

    for (GIMapperCamera *cam : m_cameras) {
        cam->GetRegion().bounds = m_bounds;
    }
}

void GIMapper::UpdateRenderTick(double dt)
{
    m_render_tick += dt;

    for (GIMapperCamera *gi_cam : m_cameras) {
        gi_cam->Update(dt); // update matrices
    }
}

void GIMapper::Bind(Shader *shader)
{
    shader->SetUniform("VoxelProbePosition", m_bounds.GetCenter());
    shader->SetUniform("VoxelSceneScale", m_bounds.GetDimensions());
}

void GIMapper::Render(Renderer *renderer, Camera *cam)
{
    if (m_is_first_run) {
        if (!Environment::GetInstance()->VCTEnabled()) {
            return;
        }

        for (GIMapperCamera *gi_cam : m_cameras) {
            gi_cam->Render(renderer, cam);
        }

        m_is_first_run = false;

        return;
    }

    if (m_render_tick < 1.0) {
        return;
    }

    m_render_tick = 0.0;

    if (!Environment::GetInstance()->VCTEnabled()) {
        return;
    }

    m_cameras[m_render_index]->Render(renderer, cam);

    m_render_index++;
    m_render_index = m_render_index % m_cameras.size();
}
} // namespace apex