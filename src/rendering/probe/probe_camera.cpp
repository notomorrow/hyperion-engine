#include "probe_camera.h"

#include "../renderer.h"
#include "../texture_3D.h"
#include "../camera/perspective_camera.h"

namespace hyperion {
ProbeCamera::ProbeCamera(const ProbeRegion &region, int width, int height, float near, float far)
    : Renderable(fbom::FBOMObjectType("PROBE_CAMERA"), RB_BUFFER),
      m_region(region),
      m_camera(new PerspectiveCamera(90.0f, width, height, near, far))
{
}

ProbeCamera::~ProbeCamera()
{
    delete m_camera;
}

void ProbeCamera::Update(double dt)
{
    m_camera->SetTranslation(m_region.origin);
    m_camera->SetDirection(m_region.direction);
    m_camera->SetUpVector(m_region.up_vector);
    m_camera->Update(dt);
}

void ProbeCamera::Render(Renderer *renderer, Camera *)
{
    // no-op
}

std::shared_ptr<Renderable> ProbeCamera::CloneImpl()
{
    return std::make_shared<ProbeCamera>(m_region, m_camera->GetWidth(), m_camera->GetHeight(), m_camera->GetNear(), m_camera->GetFar());
}
} // namespace hyperion