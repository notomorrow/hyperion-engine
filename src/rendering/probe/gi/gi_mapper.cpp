#include "gi_mapper.h"

#include "../../shader_manager.h"
#include "../../shaders/gi/gi_voxel_shader.h"
#include "../../shaders/gi/gi_voxel_clear_shader.h"
#include "../../shaders/compute/blur_compute_shader.h"
#include "../../texture_3D.h"
#include "../../renderer.h"
#include "../../environment.h"
#include "../../../math/math_util.h"

#include "../../../opengl.h"
#include "../../../gl_util.h"

namespace hyperion {
GIMapper::GIMapper(const Vector3 &origin, const BoundingBox &bounds)
    : Probe(fbom::FBOMObjectType("GI_MAPPER"), ProbeType::PROBE_TYPE_VCT, origin, bounds),
      m_render_tick(0.0),
      m_render_index(0),
      m_is_first_run(true)
{
    SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelShader>(ShaderProperties()));

    m_previous_origin = m_origin;

    m_rendered_texture.reset(new Texture3D(ProbeManager::voxel_map_size, ProbeManager::voxel_map_size, ProbeManager::voxel_map_size, nullptr));
    m_rendered_texture->SetWrapMode(CoreEngine::GLEnums::CLAMP_TO_EDGE, CoreEngine::GLEnums::CLAMP_TO_EDGE);
    m_rendered_texture->SetFilter(CoreEngine::GLEnums::LINEAR, CoreEngine::GLEnums::LINEAR_MIPMAP_LINEAR);
    m_rendered_texture->SetFormat(Texture::TextureBaseFormat::TEXTURE_FORMAT_RGBA);
    m_rendered_texture->SetInternalFormat(Texture::TextureInternalFormat::TEXTURE_INTERNAL_FORMAT_RGBA32F);

    m_clear_shader = ShaderManager::GetInstance()->GetShader<GIVoxelClearShader>(ShaderProperties());
    m_mipmap_shader = ShaderManager::GetInstance()->GetShader<BlurComputeShader>(
        ShaderProperties()
        //.Define(std::string("DIRECTION_") + std::to_string(region.index), true)
    );

    for (int i = 0; i < m_cameras.size(); i++) {
        ProbeRegion region;
        region.origin = m_origin;
        region.bounds = m_bounds;
        region.direction = m_directions[i].first;
        region.up_vector = m_directions[i].second;
        region.index = i;

        m_cameras[i] = new GIMapperCamera(region);
        m_cameras[i]->SetShader(m_shader);
    }
}

GIMapper::~GIMapper()
{
    for (ProbeCamera *cam : m_cameras) {
        if (cam == nullptr) {
            continue;
        }

        delete cam;
    }
}

void GIMapper::Update(double dt)
{
    m_render_tick += dt;

    for (ProbeCamera *gi_cam : m_cameras) {
        gi_cam->Update(dt); // update matrices
    }
}

void GIMapper::Render(Renderer *renderer, Camera *cam)
{
    if (!ProbeManager::GetInstance()->VCTEnabled()) {
        return;
    }

    if (!m_rendered_texture->IsUploaded()) {
        m_rendered_texture->Begin(false); // do not upload texture data
        // this ought to be refactored into a more reusable format
        glTexStorage3D(GL_TEXTURE_3D, ProbeManager::voxel_map_num_mipmaps + 1, GL_RGBA32F, ProbeManager::voxel_map_size, ProbeManager::voxel_map_size, ProbeManager::voxel_map_size);
        m_rendered_texture->End();
    }
    if (m_render_tick < 1.0) {
        return;
    }

    m_render_tick = 0.0;

    //if (m_is_first_run) {

        glBindImageTexture(0, m_rendered_texture->GetId(), 0, true, 0, GL_WRITE_ONLY, GL_RGBA32F);

        m_clear_shader->Use();
        m_clear_shader->Dispatch(ProbeManager::voxel_map_size, ProbeManager::voxel_map_size, ProbeManager::voxel_map_size);
        m_clear_shader->End();

        for (int i = 0; i < 6; i++) {
            m_cameras[i]->Render(renderer, cam);
        }

        glBindImageTexture(0, 0, 0, true, 0, GL_WRITE_ONLY, GL_RGBA32F);

        m_is_first_run = false;

        m_mipmap_shader->SetUniform(m_mipmap_shader->m_uniform_src_texture, m_rendered_texture.get());

        for (int i = 1; i < ProbeManager::voxel_map_num_mipmaps + 1; i++) {
            int mip_size = ProbeManager::voxel_map_size >> i;
            m_mipmap_shader->SetUniform(m_mipmap_shader->m_uniform_src_mip_level, i - 1);
            glBindImageTexture(0, m_rendered_texture->GetId(), i, true, 0, GL_WRITE_ONLY, GL_RGBA32F);
            m_mipmap_shader->Use();
            m_mipmap_shader->Dispatch(mip_size, mip_size, mip_size);
            m_mipmap_shader->End();
        }

        return;
    //}

    //if (m_render_tick < 1.0) {
    //    return;
    //}

    //m_render_tick = 0.0;

    m_cameras[m_render_index]->Render(renderer, cam);

    m_render_index++;
    m_render_index = m_render_index % m_cameras.size();

    if (m_render_index == 0) {
        m_previous_origin = m_origin;
    }
}

std::shared_ptr<Renderable> GIMapper::CloneImpl()
{
    return std::make_shared<GIMapper>(m_origin, m_bounds);
}
} // namespace apex