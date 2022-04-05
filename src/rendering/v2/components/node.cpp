#include "node.h"
#include <system/debug.h>

#include "rendering/v2/engine.h"

namespace hyperion::v2 {

Node::Node(const char *tag)
    : m_parent_node(nullptr),
      m_spatial(nullptr)
{
    size_t len = std::strlen(tag);
    m_tag = new char[len + 1];
    std::strcpy(m_tag, tag);
}

Node::~Node()
{
    delete[] m_tag;
}

void Node::UpdateWorldTransform()
{
    if (m_parent_node != nullptr) {
        m_world_transform = m_parent_node->GetWorldTransform() * m_local_transform;
    } else {
        m_world_transform = m_local_transform;
    }

    m_world_aabb = m_local_aabb * m_world_transform;

    for (auto &node : m_child_nodes) {
        node->UpdateWorldTransform();

        m_world_aabb.Extend(node->m_world_aabb);
    }
}

void Node::OnNestedNodeAdded(Node *node)
{
    m_internal_nested_children.push_back(node);
    
    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeAdded(node);
    }
}

void Node::OnNestedNodeRemoved(Node *node)
{
    const auto it = std::find(m_internal_nested_children.begin(), m_internal_nested_children.end(), node);

    if (it != m_internal_nested_children.end()) {
        m_internal_nested_children.erase(it);
    }

    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeRemoved(node);
    }
}

void Node::AddChild(std::unique_ptr<Node> &&node)
{
    AssertThrow(node != nullptr);
    AssertThrow(node->m_parent_node == nullptr);

    node->m_parent_node = this;

    OnNestedNodeAdded(node.get());

    for (Node *nested : node->GetNestedChildren()) {
        OnNestedNodeAdded(nested);
    }

    m_child_nodes.push_back(std::move(node));
}

bool Node::RemoveChild(NodeList::iterator iter)
{
    if (iter == m_child_nodes.end()) {
        return false;
    }

    Node *node = iter->get();

    AssertThrow(node != nullptr);
    AssertThrow(node->m_parent_node == this);

    node->m_parent_node = nullptr;

    OnNestedNodeRemoved(node);

    for (Node *nested : node->GetNestedChildren()) {
        OnNestedNodeRemoved(nested);
    }

    m_child_nodes.erase(iter);

    return true;
}

void Node::SetLocalTransform(const Transform &transform)
{
    m_local_transform = transform;
    
    UpdateWorldTransform();
}

void Node::SetSpatial(Engine *engine, Spatial *spatial)
{
    if (m_spatial == spatial) {
        return;
    }

    m_spatial = spatial;

    if (spatial != nullptr) {
        m_local_aabb = spatial->GetLocalAabb();
    } else {
        m_local_aabb = BoundingBox();
    }

    UpdateWorldTransform();
}

void Node::UpdateSpatialTransform(Engine *engine)
{
    engine->SetSpatialTransform(m_spatial, m_world_transform);
}

void Node::Update(Engine *engine)
{
    UpdateSpatialTransform(engine);

    for (auto *node : m_internal_nested_children) {
        node->UpdateSpatialTransform(engine);
    }
}

} // namespace hyperion::v2