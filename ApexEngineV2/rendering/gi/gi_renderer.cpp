#include "gi_renderer.h"
#include "../shader_manager.h"
#include "../shaders/gi/gi_voxel_shader.h"

namespace apex {
GIRenderer::GIRenderer(Camera *view_cam)
{
    m_gi_shader = ShaderManager::GetInstance()->GetShader<GIVoxelShader>(ShaderProperties());

    std::array<Vector3, 6> directions = {
        Vector3::UnitX(), Vector3::UnitX() * -1,
        Vector3::UnitY(), Vector3::UnitY() * -1,
        Vector3::UnitZ(), Vector3::UnitZ() * -1,
    };

    for (int i = 0; i < m_gi_map_renderers.size(); i++) {
        m_gi_map_renderers[i] = new GIMapping(view_cam, 50.0);
        m_gi_map_renderers[i]->SetLightDirection(directions[i]);
    }
}

GIRenderer::~GIRenderer()
{
    for (int i = 0; i < m_gi_map_renderers.size(); i++) {
        delete m_gi_map_renderers[i];
    }
}

void GIRenderer::Render(Renderer *renderer)
{
    for (int i = 0; i < m_gi_map_renderers.size(); i++) {
        m_gi_map_renderers[i]->Begin();

        m_gi_shader->SetUniform("u_probePos", m_gi_map_renderers[i]->GetProbePosition());

        renderer->RenderBucket(
            m_gi_map_renderers[i]->GetShadowCamera(),
            renderer->GetBucket(Renderable::RB_OPAQUE),
            m_gi_shader.get(),
            false
        );

        m_gi_map_renderers[i]->End();
    }
}
} // namespace apex
