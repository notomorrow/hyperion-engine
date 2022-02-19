#include "light_volume_grid.h"
#include "../../environment.h"
#include "../../shader_manager.h"
#include "../../shaders/sh/light_volume_renderer_shader.h"
#include "../../shaders/compute/sh_compute_shader.h"
#include "../../renderer.h"
#include "../../texture_3D.h"

#include "../../camera/perspective_camera.h"
#include "../../../math/matrix_util.h"
#include "../../../math/math_util.h"

#include "../../../core_engine.h"
#include "../../../opengl.h"
#include "../../../gl_util.h"

namespace hyperion {
const int LightVolumeGrid::cubemap_width = 2;

LightVolumeGrid::LightVolumeGrid(const Vector3 &origin, const BoundingBox &bounds, int num_probes)
    : Probe(fbom::FBOMObjectType("LIGHT_VOLUME_GRID"), origin, bounds),
      m_num_probes(num_probes),
      m_render_tick(0.0),
      m_render_index(0),
      m_is_first_run(true)
{
    SetShader(ShaderManager::GetInstance()->GetShader<LightVolumeRendererShader>(ShaderProperties()));

    const int cubemap_width_squared = cubemap_width * cubemap_width;
    const int num_probes_cubed = m_num_probes * m_num_probes * m_num_probes;
    const uint64_t grid_volume_texture_size = MathUtil::NextPowerOf2(cubemap_width_squared * 2 * m_num_probes);
    std::cout << "volume texture size: " << grid_volume_texture_size << "\n";
    m_grid_volume_texture = std::make_shared<Texture3D>(
        grid_volume_texture_size,
        grid_volume_texture_size,
        grid_volume_texture_size,
        nullptr
    );
    m_grid_volume_texture->SetFilter(CoreEngine::GLEnums::LINEAR, CoreEngine::GLEnums::LINEAR);
    m_grid_volume_texture->SetFormat(CoreEngine::GLEnums::RGBA);
    m_grid_volume_texture->SetInternalFormat(CoreEngine::GLEnums::RGBA8);

    /*m_spherical_harmonics_shader = ShaderManager::GetInstance()->GetShader<SHComputeShader>(ShaderProperties());
    m_sh_texture = std::make_shared<Texture2D>(8, 8, nullptr);
    m_sh_texture->SetFormat(CoreEngine::GLEnums::RGBA);
    m_sh_texture->SetInternalFormat(CoreEngine::GLEnums::RGBA8);
    m_sh_texture->SetFilter(CoreEngine::GLEnums::NEAREST, CoreEngine::GLEnums::NEAREST);
    m_sh_texture->SetWrapMode(CoreEngine::GLEnums::CLAMP_TO_EDGE, CoreEngine::GLEnums::CLAMP_TO_EDGE);*/

    m_light_volumes.reserve(num_probes_cubed);

    Vector3 size = bounds.GetDimensions();

    float tile_width = size.x / m_num_probes;
    float tile_height = size.y / m_num_probes;
    float tile_length = size.z / m_num_probes;
    Vector3 tile_size(tile_width, tile_height, tile_length);

    for (int x = 0; x < m_num_probes; x++) {
        for (int y = 0; y < m_num_probes; y++) {
            for (int z = 0; z < m_num_probes; z++) {

                Vector3 pos = bounds.GetMin() + Vector3(x * tile_width - tile_width / 2, y * tile_height - tile_height / 2, z * tile_length - tile_length / 2);

                LightVolumeProbe *lvp = new LightVolumeProbe(
                    pos,
                    BoundingBox(pos - tile_size * 0.5f, pos + tile_size * 0.5f),
                    Vector3(x, y, z) * 2 // extra slot for each face
                );

                lvp->SetShader(GetShader());

                m_light_volumes.push_back(lvp);
            }
        }
    }
}

LightVolumeGrid::~LightVolumeGrid()
{
    for (LightVolumeProbe *volume : m_light_volumes) {
        if (volume == nullptr) {
            continue;
        }

        delete volume;
    }
}

void LightVolumeGrid::Bind(Shader *shader)
{
    for (LightVolumeProbe *volume : m_light_volumes) {
        if (volume == nullptr) {
            continue;
        }

        volume->Bind(shader);
    }

    shader->SetUniform("LightVolumeMap", m_grid_volume_texture.get());
}

void LightVolumeGrid::Update(double dt)
{
    m_render_tick += dt;

    for (LightVolumeProbe *volume : m_light_volumes) {
        if (volume == nullptr) {
            continue;
        }
        std::cout << "volume: " << volume->m_grid_offset << "\n";

        volume->Update(dt);
    }
}

void LightVolumeGrid::Render(Renderer *renderer, Camera *cam)
{
    // bind image texture,
    // render each cubemap into the image texture at some encoded coord,
    // pass the image texture into a compute shader to generate spherical harmonics
    // (also we can choose to only re-render specific sections of the texture based on scene changes!
    // this will work better if the scene is divided into a quadtree
    // and we can probably pass into the compute shader the texels that have changed as well...)

    if (m_render_tick < 1.0 && !m_is_first_run) {
        return;
    }

    m_render_tick = 0.0;

    if (!m_grid_volume_texture->IsUploaded()) {
        m_grid_volume_texture->Begin(false); // do not upload texture data
        // this ought to be refactored into a more reusable format
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, m_grid_volume_texture->GetWidth(), m_grid_volume_texture->GetHeight(), m_grid_volume_texture->GetLength());
        CatchGLErrors("Failed to set texture storage 3d for lighting volume texture");
        m_grid_volume_texture->End();
        CatchGLErrors("Failed to end texture storage 3d for lighting volume texture");
    }
    glBindImageTexture(0, m_grid_volume_texture->GetId(), 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);
    CatchGLErrors("Failed to bind 3d image texture");

    for (LightVolumeProbe *volume : m_light_volumes) {
        if (volume == nullptr) {
            continue;
        }

        volume->Render(renderer, cam);
    }

    glBindImageTexture(0, 0, 0, false, 0, GL_WRITE_ONLY, GL_RGBA8);

    m_is_first_run = false;
}

std::shared_ptr<Renderable> LightVolumeGrid::CloneImpl()
{
    return std::make_shared<LightVolumeGrid>(m_origin, m_bounds, m_num_probes);
}
} // namespace hyperion
