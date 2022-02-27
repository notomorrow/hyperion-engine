#include "spherical_harmonics_probe.h"
#include "spherical_harmonics_probe.h"
#include "../../environment.h"
#include "../../shader_manager.h"
#include "../../shaders/cubemap_renderer_shader.h"
#include "./calculate_spherical_harmonics.h"
#include "../../renderer.h"
#include "../../../asset/byte_writer.h"

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
        }
    }
}

void SphericalHarmonicsProbe::Render(Renderer *renderer, Camera *cam)
{
    if (!ProbeManager::GetInstance()->SphericalHarmonicsEnabled()) {
        return;
    }

    if (!m_needs_rerender || m_cubemap == nullptr || !m_cubemap->IsUploaded()) {
        return;
    }

    // TODO: check error state
    std::array<Vector3, 9> sh = CalculateSphericalHarmonics(m_cubemap.get());

    const Texture::TextureBaseFormat sh_map_format = Texture::TextureBaseFormat::TEXTURE_FORMAT_RGB;
    const Texture::TextureInternalFormat sh_map_internal_format = Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGB32F;

    const size_t num_components = Texture::NumComponents(sh_map_format);

    MemoryByteWriter byte_writer;

    for (int x = 0; x < 3; x++) {
        for (int y = 0; y < 3; y++) {
            Vector3 pixel = sh[x * 3 + y];

            byte_writer.Write(pixel.x);
            byte_writer.Write(pixel.y);
            byte_writer.Write(pixel.z);

            if (num_components == 4) {
                byte_writer.Write(1.0f);
            }
        }
    }

    // NOTE: malloc is free'd by Texture destructor
    unsigned char *bytes = (unsigned char *)std::malloc(8 * 8 * num_components * sizeof(float));
    std::memcpy(bytes, byte_writer.GetData().data(), byte_writer.GetData().size());

    m_rendered_texture.reset(new Texture2D(8, 8, bytes));
    m_rendered_texture->SetFormat(sh_map_format);
    m_rendered_texture->SetInternalFormat(sh_map_internal_format);
    m_rendered_texture->SetFilter(Texture::TextureFilterMode::TEXTURE_FILTER_NEAREST);
    m_rendered_texture->SetWrapMode(CoreEngine::GLEnums::CLAMP_TO_EDGE, CoreEngine::GLEnums::CLAMP_TO_EDGE);
    m_rendered_texture->Prepare();

    m_needs_rerender = false;
}

std::shared_ptr<Renderable> SphericalHarmonicsProbe::CloneImpl()
{
    return std::make_shared<SphericalHarmonicsProbe>(m_origin, m_bounds);
}
} // namespace hyperion
