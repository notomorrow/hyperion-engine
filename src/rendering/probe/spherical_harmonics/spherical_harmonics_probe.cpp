#include "spherical_harmonics_probe.h"
#include "spherical_harmonics_probe.h"
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
SphericalHarmonicsProbe::SphericalHarmonicsProbe(const Vector3 &origin, const BoundingBox &bounds)
    : Probe(fbom::FBOMObjectType("SPHERICAL_HARMONICS_PROBE"), origin, bounds),
      m_needs_rerender(false)
{
    SetShader(ShaderManager::GetInstance()->GetShader<SHComputeShader>(ShaderProperties()));

    m_sh_texture = std::make_shared<Texture2D>(8, 8, nullptr);
    m_sh_texture->SetFormat(CoreEngine::GLEnums::RGBA);
    m_sh_texture->SetInternalFormat(CoreEngine::GLEnums::RGBA8);
    m_sh_texture->SetFilter(CoreEngine::GLEnums::NEAREST, CoreEngine::GLEnums::NEAREST);
    m_sh_texture->SetWrapMode(CoreEngine::GLEnums::CLAMP_TO_EDGE, CoreEngine::GLEnums::CLAMP_TO_EDGE);
}

SphericalHarmonicsProbe::~SphericalHarmonicsProbe()
{
}

void SphericalHarmonicsProbe::Bind(Shader *shader)
{
    if (!ProbeManager::GetInstance()->SphericalHarmonicsEnabled()) {
        shader->SetUniform("HasSphericalHarmonicsMap", 0);
        return;
    }

    shader->SetUniform("SphericalHarmonicsMap", m_sh_texture.get());
    shader->SetUniform("HasSphericalHarmonicsMap", 1);
}

void SphericalHarmonicsProbe::Update(double dt)
{

    if (const auto &nearest_cubemap = Environment::GetInstance()->GetGlobalCubemap()) {
        if (nearest_cubemap != m_cubemap) {
            m_cubemap = nearest_cubemap;
            m_needs_rerender = true;

            m_shader->SetUniform("srcTex", m_cubemap.get());
        }
    }
}

void SphericalHarmonicsProbe::Render(Renderer *renderer, Camera *cam)
{
    if (!ProbeManager::GetInstance()->SphericalHarmonicsEnabled()) {
        return;
    }

    if (!m_needs_rerender || m_cubemap == nullptr) {
        return;
    }

    if (!m_sh_texture->IsUploaded()) {
        m_sh_texture->Begin(false); // do not upload texture data
        CatchGLErrors("Failed to begin texture storage 2d for spherical harmonics");
        // this ought to be refactored into a more reusable format
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, m_sh_texture->GetWidth(), m_sh_texture->GetHeight());
        CatchGLErrors("Failed to set texture storage 2d for spherical harmonics");
        m_sh_texture->End();
        CatchGLErrors("Failed to end texture storage 2d for spherical harmonics");
    }

    glBindImageTexture(0, m_sh_texture->GetId(), 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
    CatchGLErrors("Failed to bind imagetexture");
    static_cast<ComputeShader*>(m_shader.get())->Dispatch(8, 8, 1);
    std::cout << "dispatch\n";
    glBindImageTexture(0, 0, 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);

    m_needs_rerender = false;
}

std::shared_ptr<Renderable> SphericalHarmonicsProbe::CloneImpl()
{
    return std::make_shared<SphericalHarmonicsProbe>(m_origin, m_bounds);
}
} // namespace hyperion
