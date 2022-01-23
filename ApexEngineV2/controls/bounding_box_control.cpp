#include "bounding_box_control.h"

namespace hyperion {
BoundingBoxControl::BoundingBoxControl()
    : EntityControl()
{
    m_bounding_box_renderer.reset(new BoundingBoxRenderer());

    m_entity.reset(new Entity("AABB"));
    m_entity->SetAABBAffectsParent(false);
    m_entity->SetRenderable(m_bounding_box_renderer);
}

BoundingBoxControl::~BoundingBoxControl()
{
}

void BoundingBoxControl::OnAdded()
{
    parent->AddChild(m_entity);
}

void BoundingBoxControl::OnRemoved()
{
    parent->RemoveChild(m_entity);
}

void BoundingBoxControl::OnUpdate(double dt)
{
    m_bounding_box_renderer->SetAABB(parent->GetAABB());
}
} // namespace hyperion
