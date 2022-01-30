#include "gi_renderer.h"
#include "../renderer.h"
#include "../shader_manager.h"
#include "../shaders/gi/gi_voxel_shader.h"
#include "../shaders/gi/gi_voxel_clear_shader.h"

namespace hyperion {
GIRenderer::GIRenderer()
{
    m_gi_shader = ShaderManager::GetInstance()->GetShader<GIVoxelShader>(ShaderProperties());
    m_clear_shader = ShaderManager::GetInstance()->GetShader<GIVoxelClearShader>(ShaderProperties());

    m_gi_map_renderers = { new GIMapping() };
}

GIRenderer::~GIRenderer()
{
    for (int i = 0; i < m_gi_map_renderers.size(); i++) {
        delete m_gi_map_renderers[i];
    }
}

void GIRenderer::Render(Renderer* renderer, Camera *cam)
{
    for (int i = 0; i < m_gi_map_renderers.size(); i++) {
        m_gi_map_renderers[i]->Begin();

        // clear texture of previous frame's voxels
        m_clear_shader->Use();
        m_clear_shader->Dispatch();
        m_clear_shader->End();

        m_gi_shader->SetUniform("u_probePos", Vector3(0, 0, 0));//m_gi_map_renderers[i]->GetProbePosition());

        renderer->RenderBucket(
            cam,//m_gi_map_renderers[i]->GetShadowCamera(),
            renderer->GetBucket(Renderable::RB_OPAQUE),
            m_gi_shader.get(),
            false
        );

        m_gi_map_renderers[i]->End();
    }
}
} // namespace apex