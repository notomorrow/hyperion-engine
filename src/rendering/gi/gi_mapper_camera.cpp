#include "gi_mapper_camera.h"

#include "gi_manager.h"
#include "../shader_manager.h"
#include "../shaders/gi/gi_voxel_shader.h"
#include "../shaders/gi/gi_voxel_clear_shader.h"
#include "../renderer.h"
#include "../camera/perspective_camera.h"
#include "../../math/math_util.h"
#include "../../opengl.h"
#include "../../gl_util.h"

namespace hyperion {
GIMapperCamera::GIMapperCamera(const GIMapperRegion &region)
    : Renderable(RB_BUFFER),
      m_texture_id(0),
      m_region(region),
      m_camera(new PerspectiveCamera(90.0f, GIManager::voxel_map_size, GIManager::voxel_map_size, 0.1f, 100.0f))
{
    m_clear_shader = ShaderManager::GetInstance()->GetShader<GIVoxelClearShader>(ShaderProperties());
}

GIMapperCamera::~GIMapperCamera()
{
    delete m_camera;

    if (m_texture_id != 0) {
        glDeleteTextures(1, &m_texture_id);
    }
}

void GIMapperCamera::Begin()
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
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA32F, GIManager::voxel_map_size, GIManager::voxel_map_size, GIManager::voxel_map_size);
        CatchGLErrors("Failed to set 3d texture storage.");
        glGenerateMipmap(GL_TEXTURE_3D);
        //CatchGLErrors("Failed to gen 3d texture mipmaps.");
        glBindTexture(GL_TEXTURE_3D, 0);
    }

    glBindImageTexture(0, m_texture_id, 0, true, 0, GL_READ_WRITE, GL_RGBA32F);
    CatchGLErrors("Failed to bind image texture.");
}

void GIMapperCamera::End()
{
    glBindImageTexture(0, 0, 0, true, 0, GL_READ_WRITE, GL_RGBA32F);
}

void GIMapperCamera::Update(double dt)
{
    m_camera->SetTranslation(m_region.bounds.GetCenter());
    m_camera->SetDirection(m_region.direction);
    m_camera->SetUpVector(m_region.up_vector);
    m_camera->Update(dt);
}

void GIMapperCamera::Render(Renderer *renderer, Camera *)
{
    Begin();

    // clear texture of previous frame's voxels
    m_clear_shader->Use();
    m_clear_shader->Dispatch();
    m_clear_shader->End();

    renderer->RenderBucket(
        m_camera,
        renderer->GetBucket(Renderable::RB_OPAQUE),
        m_shader.get(),
        false
    );

    End();
}
} // namespace apex