#include "gi_mapper.h"

#include "../shader_manager.h"
#include "../shaders/gi/gi_voxel_shader.h"
#include "../shaders/gi/gi_voxel_clear_shader.h"
#include "../renderer.h"
#include "../../math/math_util.h"
#include "../../opengl.h"
#include "../../gl_util.h"

namespace hyperion {
GIMapper::GIMapper(const GIMapperRegion &region)
    : Renderable(RB_BUFFER),
      m_texture_id(0),
      m_region(region),
      m_render_tick(MathUtil::MaxSafeValue<double>())
{
    SetShader(ShaderManager::GetInstance()->GetShader<GIVoxelShader>(ShaderProperties()));
    m_clear_shader = ShaderManager::GetInstance()->GetShader<GIVoxelClearShader>(ShaderProperties());
}

GIMapper::~GIMapper()
{
    if (m_texture_id != 0) {
        glDeleteTextures(1, &m_texture_id);
    }
}

void GIMapper::Bind(Shader *shader)
{
    shader->SetUniform("VoxelProbePosition", m_region.bounds.GetCenter());
    shader->SetUniform("VoxelSceneScale", m_region.bounds.GetDimensions());
}

void GIMapper::Begin()
{
    if (m_texture_id == 0) {
        glGenTextures(1, &m_texture_id);
        CatchGLErrors("Failed to generate textures.");
        glBindTexture(GL_TEXTURE_3D, m_texture_id);
        CatchGLErrors("Failed to bind 3d texture.");
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        CatchGLErrors("Failed to init 3d texture parameters.");
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32F, 128, 128, 128);
        CatchGLErrors("Failed to set 3d texture storage.");
        glGenerateMipmap(GL_TEXTURE_3D);
        //CatchGLErrors("Failed to gen 3d texture mipmaps.");
        glBindTexture(GL_TEXTURE_3D, 0);
    }

    glBindImageTexture(0, m_texture_id, 0, true, 0, GL_READ_WRITE, GL_RGBA32F);
    CatchGLErrors("Failed to bind image texture.");
}

void GIMapper::End()
{
    glBindImageTexture(0, 0, 0, true, 0, GL_READ_WRITE, GL_RGBA32F);
}

void GIMapper::UpdateRenderTick(double dt)
{
    m_render_tick += dt;
}

void GIMapper::Render(Renderer *renderer, Camera *cam)
{
    if (m_render_tick < 0.1) {
        return;
    }

    m_render_tick = 0.0;

    Begin();

    // clear texture of previous frame's voxels
    m_clear_shader->Use();
    m_clear_shader->Dispatch();
    m_clear_shader->End();

    Bind(m_shader.get());

    renderer->RenderBucket(
        cam,
        renderer->GetBucket(Renderable::RB_OPAQUE),
        m_shader.get(),
        false
    );

    End();
}
} // namespace apex