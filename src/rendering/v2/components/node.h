#ifndef HYPERION_V2_NODE_H
#define HYPERION_V2_NODE_H

#include "spatial.h"
#include <math/transform.h>

#include <vector>
#include <memory>

namespace hyperion::v2 {

class Engine;

class Node {
public:
    using NodeList = std::vector<std::unique_ptr<Node>>;

    Node(const char *tag = "");
    Node(const Node &other) = delete;
    Node &operator=(const Node &other) = delete;
    ~Node();

    const char *GetTag() const { return m_tag; }
    inline Node *GetParent() const { return m_parent_node; }

    inline Spatial *GetSpatial() const { return m_spatial; }
    void SetSpatial(Engine *engine, Spatial *spatial);

    void AddChild(std::unique_ptr<Node> &&);
    void RemoveChild(NodeList::iterator);

    inline const std::vector<Node *> &GetNestedChildren() const { return m_internal_nested_children; }

    void SetLocalTransform(const Transform &);
    const Transform &GetLocalTransform() const { return m_local_transform; }
    const Transform &GetWorldTransform() const { return m_world_transform; }

    inline const Vector3 &GetLocalTranslation() const
        { return m_local_transform.GetTranslation(); }

    inline void SetLocalTranslation(const Vector3 &translation)
        { SetLocalTransform({translation, m_local_transform.GetScale(), m_local_transform.GetRotation()}); }

    inline const Vector3 &GetLocalScale() const
        { return m_local_transform.GetScale(); }

    inline void SetLocalScale(const Vector3 &scale)
        { SetLocalTransform({m_local_transform.GetTranslation(), scale, m_local_transform.GetRotation()}); }

    inline const Quaternion &GetLocalRotation() const
        { return m_local_transform.GetRotation(); }

    inline void SetLocalRotation(const Quaternion &rotation)
        { SetLocalTransform({m_local_transform.GetTranslation(), m_local_transform.GetScale(), rotation}); }

    void Update(Engine *engine);

private:
    void UpdateWorldTransform();
    void OnNestedNodeAdded(Node *node);
    void OnNestedNodeRemoved(Node *node);
    void UpdateSpatialTransform(Engine *engine);

    char *m_tag;
    Node *m_parent_node;
    NodeList m_child_nodes;
    Transform m_local_transform;
    Transform m_world_transform;
    BoundingBox m_local_aabb;
    BoundingBox m_world_aabb;

    Spatial *m_spatial;

    std::vector<Node *> m_internal_nested_children;
};

} // namespace hyperion::v2

#endif