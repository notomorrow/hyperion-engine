#include "light_volume_grid_control.h"
#include "light_volume_grid.h"
#include "../probe_manager.h"
#include "../../../entity.h"

#include "../../../rendering/renderers/bounding_box_renderer.h"

#include <memory>

namespace hyperion {
LightVolumeGridControl::LightVolumeGridControl(const Vector3 &origin, BoundingBox bounds)
    : EntityControl(fbom::FBOMObjectType("LIGHT_VOLUME_GRID_CONTROL"), 5.0),
      m_light_volume_grid_node(new Entity("LightVolumeGrid")),
      m_light_volume_grid(new LightVolumeGrid(origin, bounds, 3)) // TODO
{
    m_light_volume_grid_node->SetRenderable(m_light_volume_grid);
}

void LightVolumeGridControl::OnAdded()
{
    parent->AddChild(m_light_volume_grid_node);
    ProbeManager::GetInstance()->AddProbe(m_light_volume_grid);

    for (auto it : m_light_volume_grid->m_light_volumes) {
        auto node = std::make_shared<Entity>();
        node->SetRenderable(std::make_shared<BoundingBoxRenderer>(it->GetBounds()));
        parent->AddChild(node);
    }
}

void LightVolumeGridControl::OnRemoved()
{
    parent->RemoveChild(m_light_volume_grid_node);
    ProbeManager::GetInstance()->RemoveProbe(m_light_volume_grid);
}

void LightVolumeGridControl::OnUpdate(double dt)
{
    if (m_light_volume_grid->GetBounds().Empty()) {
        m_light_volume_grid->SetBounds(parent->GetAABB());
    }

    m_light_volume_grid->SetOrigin(parent->GetGlobalTranslation());
    m_light_volume_grid->Update(dt);
}

std::shared_ptr<Control> LightVolumeGridControl::CloneImpl()
{
    return std::make_shared<LightVolumeGridControl>(m_light_volume_grid->GetOrigin(), m_light_volume_grid->GetBounds());
}
} // namespace hyperion