#include "node.h"
#include <system/debug.h>

#include "rendering/v2/engine.h"

namespace hyperion::v2 {

Node::Node(const char *tag, const Transform &local_transform)
    : Node(tag, nullptr, local_transform)
{
}

Node::Node(const char *tag, Ref<Spatial> &&spatial, const Transform &local_transform)
    : m_parent_node(nullptr),
      m_local_transform(local_transform)
{
    SetSpatial(std::move(spatial));

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
    m_descendents.push_back(node);
    
    if (m_parent_node != nullptr) {
        m_parent_node->OnNestedNodeAdded(node);
    }
}

void Node::OnNestedNodeRemoved(Node *node)
{
    const auto it = std::find(m_descendents.begin(), m_descendents.end(), node);

    if (it != m_descendents.end()) {
        m_descendents.erase(it);
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

    for (Node *nested : node->GetDescendents()) {
        OnNestedNodeAdded(nested);
    }

    m_child_nodes.push_back(std::move(node));
}

bool Node::RemoveChild(NodeList::iterator iter)
{
    if (iter == m_child_nodes.end()) {
        return false;
    }

    if (Node *node = iter->get()) {
        AssertThrow(node->m_parent_node == this);

        for (Node *nested : node->GetDescendents()) {
            OnNestedNodeRemoved(nested);
        }

        OnNestedNodeRemoved(node);

        node->m_parent_node = nullptr;
    }

    m_child_nodes.erase(iter);

    return true;
}

bool Node::RemoveChild(size_t index)
{
    if (index >= m_child_nodes.size()) {
        return false;
    }

    return RemoveChild(m_child_nodes.begin() + index);
}

bool Node::Remove()
{
    if (m_parent_node == nullptr) {
        return false;
    }

    return m_parent_node->RemoveChild(m_parent_node->FindChild(this));
}

Node *Node::GetChild(size_t index) const
{
    if (index >= m_child_nodes.size()) {
        return nullptr;
    }

    return m_child_nodes[index].get();
}

Node::NodeList::iterator Node::FindChild(Node *node)
{
    return std::find_if(
        m_child_nodes.begin(),
        m_child_nodes.end(),
        [node](const auto &it) {
            return it.get() == node;
        });
}

void Node::SetLocalTransform(const Transform &transform)
{
    m_local_transform = transform;
    
    UpdateWorldTransform();
}

void Node::SetSpatial(Ref<Spatial> &&spatial)
{
    if (m_spatial == spatial) {
        return;
    }

    if (spatial != nullptr) {
        m_spatial = std::move(spatial);
        m_spatial.Init();

        m_local_aabb = m_spatial->GetLocalAabb();
    } else {
        m_local_aabb = BoundingBox();

        m_spatial = nullptr;
    }

    UpdateWorldTransform();
}

void Node::UpdateSpatialTransform(Engine *engine)
{
    if (m_spatial != nullptr) {
        engine->SetSpatialTransform(m_spatial, m_world_transform);
    }
}

void Node::Update(Engine *engine)
{
    UpdateSpatialTransform(engine);

    for (auto *node : m_descendents) {
        node->UpdateSpatialTransform(engine);
    }
}

} // namespace hyperion::v2