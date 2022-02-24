#include "gi_mapper_camera.h"

#include "../probe_manager.h"
#include "../../shader_manager.h"
#include "../../shaders/gi/gi_voxel_shader.h"
#include "../../shaders/gi/gi_voxel_clear_shader.h"
#include "../../shaders/compute/blur_compute_shader.h"
#include "../../renderer.h"
#include "../../texture_3D.h"
#include "../../camera/ortho_camera.h"
#include "../../camera/perspective_camera.h"
#include "../../../math/math_util.h"
#include "../../../opengl.h"
#include "../../../gl_util.h"

namespace hyperion {
GIMapperCamera::GIMapperCamera(const ProbeRegion &region)
    : ProbeCamera(fbom::FBOMObjectType("GI_MAPPER_CAMERA"), region)
{
    m_camera = new PerspectiveCamera(90.0f, ProbeManager::voxel_map_size, ProbeManager::voxel_map_size, 0.05f, float(ProbeManager::voxel_map_size) / ProbeManager::voxel_map_scale);

        //new OrthoCamera(-ProbeManager::voxel_map_size, ProbeManager::voxel_map_size, -ProbeManager::voxel_map_size, ProbeManager::voxel_map_size, 0, ProbeManager::voxel_map_size);

}

GIMapperCamera::~GIMapperCamera()
{
    delete m_camera;
}

void GIMapperCamera::Update(double dt)
{
    m_camera->SetTranslation(m_region.origin);
    m_camera->SetDirection(m_region.direction);
    m_camera->SetUpVector(m_region.up_vector);
    m_camera->Update(dt);
}

void GIMapperCamera::Render(Renderer *renderer, Camera *)
{
    SceneManager::GetInstance()->GetOctree()->UpdateVisibilityState(
        Octree::VisibilityState::CameraType(Octree::VisibilityState::CameraType::VIS_CAMERA_VOXEL0 + m_region.index),
        m_camera->GetFrustum()
    );

    renderer->RenderBucket(
        m_camera,
        Spatial::Bucket::RB_OPAQUE,
        Octree::VisibilityState::CameraType(Octree::VisibilityState::CameraType::VIS_CAMERA_VOXEL0 + m_region.index),
        m_shader.get()
    );
}

std::shared_ptr<Renderable> GIMapperCamera::CloneImpl()
{
    return std::make_shared<GIMapperCamera>(m_region);
}
} // namespace hyperion