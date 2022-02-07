#include "probe_renderer.h"
#include "../framebuffer_cube.h"
#include "../shader_manager.h"
#include "../shaders/cubemap_renderer_shader.h"

namespace hyperion {
ProbeRenderer::ProbeRenderer(const Vector3 &origin, const BoundingBox &bounds, int width, int height)
    : m_fbo(new FramebufferCube(width, height)),
      m_render_shading(false),
      m_render_textures(false)
{
    m_cubemap_renderer_shader = ShaderManager::GetInstance()->GetShader<CubemapRendererShader>(ShaderProperties());

    m_probe = new Probe(
        origin,
        bounds,
        width,
        height,
        0.1f,
        45.0f //todo
    );
}

ProbeRenderer::~ProbeRenderer()
{
    delete m_probe;
}

void ProbeRenderer::UpdateUniforms()
{
    const auto &cameras = m_probe->GetCameras();

    for (int i = 0; i < cameras.size(); i++) {
        m_cubemap_renderer_shader->SetUniform("u_shadowMatrices[" + std::to_string(i) + "]", cameras[i]->GetCamera()->GetViewProjectionMatrix());
    }

    //m_cubemap_renderer_shader->SetUniform("u_lightPos", m_probe->GetOrigin());
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

    // renderer->RenderBucket(
    //     cam,
    //     renderer->GetBucket(Renderable::RB_SKY),
    //     m_cubemap_renderer_shader.get(),
    //     false
    // );

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

void ProbeRenderer::SetOrigin(const Vector3 &origin)
{
    hard_assert(m_probe != nullptr);

    m_probe->SetOrigin(origin);
}
} // namespace hyperion
