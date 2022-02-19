#include "spherical_harmonics_control.h"
#include "spherical_harmonics_probe.h"
#include "../probe_manager.h"
#include "../../../scene/node.h"

#include <memory>

namespace hyperion {
SphericalHarmonicsControl::SphericalHarmonicsControl(const Vector3 &origin, BoundingBox bounds)
    : EntityControl(fbom::FBOMObjectType("SPHERICAL_HARMONICS_CONTROL"), 1.0),
      m_origin(origin),
      m_bounds(bounds)
{
}

void SphericalHarmonicsControl::OnAdded()
{
    m_probe.reset(new SphericalHarmonicsProbe(m_origin, m_bounds));
    m_node.reset(new Node("SphericalHarmonicsControl"));
    m_node->SetRenderable(m_probe);
    m_node->GetSpatial().SetBucket(Spatial::Bucket::RB_BUFFER);

    parent->AddChild(m_node);
    ProbeManager::GetInstance()->AddProbe(m_probe);
}

void SphericalHarmonicsControl::OnRemoved()
{
    parent->RemoveChild(m_node);
    ProbeManager::GetInstance()->RemoveProbe(m_probe);
}

void SphericalHarmonicsControl::OnUpdate(double dt)
{
    if (m_probe->GetBounds().Empty()) {
        m_probe->SetBounds(parent->GetAABB());
    }

    m_probe->SetOrigin(parent->GetGlobalTranslation());
    m_probe->Update(dt);
}

std::shared_ptr<Control> SphericalHarmonicsControl::CloneImpl()
{
    return std::make_shared<SphericalHarmonicsControl>(m_origin, m_bounds);
}
} // namespace hyperion