#include "NodeProxy.hpp"
#include "Node.hpp"

namespace hyperion::v2 {

SizeType NodeProxyChildren::Size() const
{
    return node ? node->GetChildren().Size() : 0;
}

NodeProxyChildren::Iterator NodeProxyChildren::End()
{
    return Iterator { node, node->GetChildren().Size() };
}

NodeProxy &NodeProxyChildren::Iterator::operator*()
{
    AssertThrow(node != nullptr && index < node->GetChildren().Size());

    return node->GetChildren()[index];
}

const NodeProxy &NodeProxyChildren::Iterator::operator*() const
{
    return const_cast<Iterator *>(this)->operator*();
}

NodeProxy *NodeProxyChildren::Iterator::operator->()
{
    return &node->GetChildren()[index];
}

const NodeProxy *NodeProxyChildren::Iterator::operator->() const
{
    return const_cast<Iterator *>(this)->operator->();
}

NodeProxy::NodeProxy()
    : m_node(nullptr)
{
}

NodeProxy::NodeProxy(Node *node)
    : m_node(node)
{
    if (m_node) {
        m_node->IncRef();
    }
}

NodeProxy::NodeProxy(const NodeProxy &other)
    : m_node(other.m_node)
{
    if (m_node) {
        m_node->IncRef();
    }
}

NodeProxy &NodeProxy::operator=(const NodeProxy &other)
{
    ReleaseNode();

    m_node = other.m_node;

    if (m_node) {
        m_node->IncRef();
    }

    return *this;
}

NodeProxy::NodeProxy(NodeProxy &&other) noexcept
    : m_node(other.m_node)
{
    other.m_node = nullptr;
}

NodeProxy &NodeProxy::operator=(NodeProxy &&other) noexcept
{
    ReleaseNode();

    m_node = other.m_node;
    other.m_node = nullptr;

    return *this;
}

NodeProxy::~NodeProxy()
{
    ReleaseNode();
}

void NodeProxy::ReleaseNode()
{
    if (m_node == nullptr) {
        return;
    }

    m_node->DecRef();

    if (m_node->m_ref_count.count == 0) {
        delete m_node;
    }

    m_node = nullptr;
}

NodeProxy NodeProxy::GetChild(SizeType index)
{
    if (m_node && index < m_node->GetChildren().Size()) {
        return NodeProxy(m_node->GetChild(index));
    }

    return NodeProxy();
}

NodeProxy NodeProxy::Select(const char *selector) const
{
    if (m_node) {
        return NodeProxy(m_node->Select(selector));
    }

    return NodeProxy();
}

NodeProxy NodeProxy::AddChild()
{
    if (m_node) {
        return m_node->AddChild();
    }

    return NodeProxy();
}

NodeProxy NodeProxy::AddChild(const NodeProxy &node)
{
    AssertThrow(node != *this);

    if (m_node) {
        return m_node->AddChild(node);
    }

    return NodeProxy();
}

const Transform &NodeProxy::GetLocalTransform() const
{
    if (m_node) {
        return m_node->GetLocalTransform();
    }

    return Transform::identity;
}

const Vector3 &NodeProxy::GetLocalTranslation() const
    { return GetLocalTransform().GetTranslation(); }

void NodeProxy::SetLocalTranslation(const Vector3 &translation)
{
    if (m_node) {
        m_node->SetLocalTranslation(translation);
    }
}

const Vector3 &NodeProxy::GetLocalScale() const
    { return GetLocalTransform().GetScale(); }

void NodeProxy::SetLocalScale(const Vector3 &scale)
{
    if (m_node) {
        m_node->SetLocalScale(scale);
    }
}

const Quaternion &NodeProxy::GetLocalRotation() const
    { return GetLocalTransform().GetRotation(); }

void NodeProxy::SetLocalRotation(const Quaternion &rotation)
{
    if (m_node) {
        m_node->SetLocalRotation(rotation);
    }
}

const Transform &NodeProxy::GetWorldTransform() const
{
    if (m_node) {
        return m_node->GetWorldTransform();
    }

    return Transform::identity;
}

const Vector3 &NodeProxy::GetWorldTranslation() const
    { return GetWorldTransform().GetTranslation(); }

void NodeProxy::SetWorldTranslation(const Vector3 &translation)
{
    if (m_node) {
        m_node->SetWorldTranslation(translation);
    }
}

const Vector3 &NodeProxy::GetWorldScale() const
    { return GetWorldTransform().GetScale(); }

void NodeProxy::SetWorldScale(const Vector3 &scale)
{
    if (m_node) {
        m_node->SetWorldScale(scale);
    }
}

const Quaternion &NodeProxy::GetWorldRotation() const
    { return GetWorldTransform().GetRotation(); }

void NodeProxy::SetWorldRotation(const Quaternion &rotation)
{
    if (m_node) {
        m_node->SetWorldRotation(rotation);
    }
}

const BoundingBox &NodeProxy::GetLocalAABB() const
{
    if (m_node) {
        return m_node->GetLocalAabb();
    }

    return BoundingBox::empty;
}

const BoundingBox &NodeProxy::GetWorldAABB() const
{
    if (m_node) {
        return m_node->GetWorldAabb();
    }

    return BoundingBox::empty;
}

} // namespace hyperion::v2