#include "probe_renderer.h"
#include "../framebuffer_cube.h"
#include "../shader_manager.h"
#include "../shaders/cubemap_renderer_shader.h"

namespace apex {
ProbeRenderer::ProbeRenderer(int width, int height)
    : m_fbo(new FramebufferCube(width, height)),
      m_render_shading(false),
      m_render_textures(false)
{
    m_cubemap_renderer_shader = ShaderManager::GetInstance()->GetShader<CubemapRendererShader>(ShaderProperties());

    m_probe = new Probe(
        Vector3(0, 0, 0),
        width,
        height,
        0.1f,
        10.0f
    );
}

ProbeRenderer::~ProbeRenderer()
{
    delete m_probe;
}

void ProbeRenderer::UpdateUniforms()
{
    const auto &matrices = m_probe->GetMatrices();

    for (int i = 0; i < matrices.size(); i++) {
        m_cubemap_renderer_shader->SetUniform("u_shadowMatrices[" + std::to_string(i) + "]", matrices[i]);
    }

    m_cubemap_renderer_shader->SetUniform("u_lightPos", m_probe->GetOrigin());
    m_cubemap_renderer_shader->SetUniform("u_far", m_probe->GetFar());
}

void ProbeRenderer::Render(Renderer *renderer, Camera *cam)
{
    m_fbo->Use();
    m_probe->Begin();

    CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

    UpdateUniforms();

    renderer->RenderBucket(
        cam,
        renderer->GetBucket(Renderable::RB_OPAQUE),
        m_cubemap_renderer_shader.get(),
        false
    );

    renderer->RenderBucket(
        cam,
        renderer->GetBucket(Renderable::RB_TRANSPARENT),
        m_cubemap_renderer_shader.get(),
        false
    );

    m_probe->End();
    m_fbo->End();
}

void ProbeRenderer::SetRenderShading(bool value)
{
    if (value == m_render_shading) {
        return;
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("PROBE_RENDER_SHADING", value)
    );

    m_render_shading = value;
}

void ProbeRenderer::SetRenderTextures(bool value)
{
    if (value == m_render_textures) {
        return;
    }

    ShaderManager::GetInstance()->SetBaseShaderProperties(
        ShaderProperties().Define("PROBE_RENDER_TEXTURES", value)
    );

    m_render_textures = value;
}
} // namespace apex
