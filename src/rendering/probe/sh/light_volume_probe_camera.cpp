#include "light_volume_probe_camera.h"

#include "../../renderer.h"
#include "../../camera/perspective_camera.h"

namespace hyperion {
LightVolumeProbeCamera::LightVolumeProbeCamera(const ProbeRegion &region, int width, int height, float near, float far)
    : ProbeCamera(fbom::FBOMObjectType("LIGHT_VOLUME_PROBE_CAMERA"), region)
{
    m_camera = new PerspectiveCamera(90.0f, width, height, near, far);
}

LightVolumeProbeCamera::~LightVolumeProbeCamera()
{
    delete m_camera;
}

void LightVolumeProbeCamera::Update(double dt)
{
    m_camera->SetTranslation(m_region.origin);
    m_camera->SetDirection(m_region.direction);
    m_camera->SetUpVector(m_region.up_vector);
    m_camera->Update(dt);
}

void LightVolumeProbeCamera::Render(Renderer *renderer, Camera *camera)
{
}

std::shared_ptr<Renderable> LightVolumeProbeCamera::CloneImpl()
{
    return std::make_shared<LightVolumeProbeCamera>(m_region, m_camera->GetWidth(), m_camera->GetHeight(), m_camera->GetNear(), m_camera->GetFar());
}
} // namespace hyperion