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
    : Probe(fbom::FBOMObjectType("SPHERICAL_HARMONICS_PROBE"), ProbeType::PROBE_TYPE_SH, origin, bounds),
      m_needs_rerender(false)
{
    m_spherical_harmonics_shader = ShaderManager::GetInstance()->GetShader<SHComputeShader>(ShaderProperties());

    m_rendered_texture = std::make_shared<Texture2D>(8, 8, nullptr);
    m_rendered_texture->SetFormat(Texture::TextureBaseFormat::TEXTURE_FORMAT_RGBA);
    m_rendered_texture->SetInternalFormat(Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA8);
    m_rendered_texture->SetFilter(Texture::TextureFilterMode::TEXTURE_FILTER_NEAREST);
    m_rendered_texture->SetWrapMode(CoreEngine::GLEnums::CLAMP_TO_EDGE, CoreEngine::GLEnums::CLAMP_TO_EDGE);
}

SphericalHarmonicsProbe::~SphericalHarmonicsProbe()
{
}

void SphericalHarmonicsProbe::Update(double dt)
{

    if (const auto &nearest_cubemap = Environment::GetInstance()->GetGlobalCubemap()) {
        if (nearest_cubemap != m_cubemap) {
            m_cubemap = nearest_cubemap;
            m_needs_rerender = true;

            //m_sh_shader->SetUniform(m_sh_shader->m_uniform_src_texture, m_cubemap.get());
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

    if (!m_rendered_texture->IsUploaded()) {
        m_rendered_texture->Begin(false); // do not upload texture data
        CatchGLErrors("Failed to begin texture storage 2d for spherical harmonics");
        // this ought to be refactored into a more reusable format
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, m_rendered_texture->GetWidth(), m_rendered_texture->GetHeight());
        CatchGLErrors("Failed to set texture storage 2d for spherical harmonics");
        m_rendered_texture->End();
        CatchGLErrors("Failed to end texture storage 2d for spherical harmonics");
    }

    glBindImageTexture(0, m_rendered_texture->GetId(), 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
    CatchGLErrors("Failed to bind imagetexture");

    m_spherical_harmonics_shader->SetUniform(
        m_spherical_harmonics_shader->m_uniform_src_texture,
        m_cubemap.get()
    );
    m_spherical_harmonics_shader->Use();
    m_spherical_harmonics_shader->Dispatch(8, 8, 1);
    m_spherical_harmonics_shader->End();

    glBindImageTexture(0, 0, 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);

    m_needs_rerender = false;
}

std::shared_ptr<Renderable> SphericalHarmonicsProbe::CloneImpl()
{
    return std::make_shared<SphericalHarmonicsProbe>(m_origin, m_bounds);
}
} // namespace hyperion
