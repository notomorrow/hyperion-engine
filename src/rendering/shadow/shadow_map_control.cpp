#include "shadow_map_control.h"
#include "shadow_mapping.h"
#include "../../scene/node.h"
#include "../../scene/scene_manager.h"

#include <memory>

namespace hyperion {
ShadowMapControl::ShadowMapControl(const Vector3 &direction, double max_dist)
    : EntityControl(fbom::FBOMObjectType("SHADOW_MAP_CONTROL"), 5.0),
      m_direction(direction),
      m_max_dist(max_dist)
{
}

void ShadowMapControl::OnAdded()
{
    m_shadow_map_renderer.reset(new ShadowMapping(m_max_dist));
    m_shadow_map_renderer->SetOrigin(parent->GetGlobalTranslation());
    m_shadow_map_renderer->SetLightDirection(m_direction);

    m_node.reset(new Node("ShadowMapControl"));
    m_node->SetRenderable(m_shadow_map_renderer);
    m_node->GetSpatial().SetBucket(Spatial::Bucket::RB_BUFFER);

    parent->AddChild(m_node);
}

void ShadowMapControl::OnRemoved()
{
    parent->RemoveChild(m_node);
}

void ShadowMapControl::OnUpdate(double dt)
{
    m_shadow_map_renderer->SetOrigin(m_node->GetGlobalTranslation());
}

std::shared_ptr<Control> ShadowMapControl::CloneImpl()
{
    return std::make_shared<ShadowMapControl>(m_direction, m_max_dist);
}
} // namespace hyperion