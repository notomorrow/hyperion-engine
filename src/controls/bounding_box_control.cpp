#include "bounding_box_control.h"

namespace hyperion {
BoundingBoxControl::BoundingBoxControl()
    : EntityControl(fbom::FBOMObjectType("BOUNDING_BOX_CONTROL"))
{
    m_bounding_box_renderer.reset(new BoundingBoxRenderer());

    m_node.reset(new Node("AABB"));
    m_node->SetAABBAffectsParent(false);
    m_node->SetRenderable(m_bounding_box_renderer);
    m_node->GetSpatial().SetBucket(Spatial::Bucket::RB_DEBUG);
}

BoundingBoxControl::~BoundingBoxControl()
{
}

void BoundingBoxControl::OnAdded()
{
    parent->AddChild(m_node);
}

void BoundingBoxControl::OnRemoved()
{
    parent->RemoveChild(m_node);
}

void BoundingBoxControl::OnUpdate(double dt)
{
    m_bounding_box_renderer->SetAABB(parent->GetAABB());
}

std::shared_ptr<Control> BoundingBoxControl::CloneImpl()
{
    return std::make_shared<BoundingBoxControl>();
}
} // namespace hyperion
