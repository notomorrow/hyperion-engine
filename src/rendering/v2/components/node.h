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

    /*! \brief Construct the node, optionally taking in a string tag to improve identification.
     * @param tag A c-string representing the name of the Node. The memory is copied internally so the string can be safely deleted
     * after use.
     */
    Node(const char *tag = "");
    Node(const Node &other) = delete;
    Node &operator=(const Node &other) = delete;
    ~Node();

    /*! @returns The string tag that was given to the Node on creation. */
    const char *GetTag() const { return m_tag; }
    /*! @returns A pointer to the parent Node of this Node. May be null. */
    inline Node *GetParent() const { return m_parent_node; }

    inline Spatial *GetSpatial() const { return m_spatial; }
    void SetSpatial(Engine *engine, Spatial *spatial);

    /*! \brief Add the Node as a child of this object, taking ownership over the given Node.
     * @param node The Node to be added as achild of this Node
     */
    void AddChild(std::unique_ptr<Node> &&node);

    /*! \brief Add a new child Node to this object, using the passed arguments as constructor arguments.
     * @param args The arguments to be passed to the new Node's constructor
     * @returns A pointer to the child Node, owned by this Node.
     */
    template <class ...Args>
    Node *AddChild(Args &&... args)
    {
        AddChild(std::make_unique<Node>(std::forward<Args>(args)...));

        return m_child_nodes.back().get();
    }

    bool RemoveChild(NodeList::iterator);

    inline NodeList &GetChildren() { return m_child_nodes; }
    inline const NodeList &GetChildren() const { return m_child_nodes; }
    inline const std::vector<Node *> &GetNestedChildren() const { return m_internal_nested_children; }

    /*! \brief Set the local-space translation, scale, rotation of this Node (not influenced by the parent Node) */
    void SetLocalTransform(const Transform &);

    /*! @returns The local-space translation, scale, rotation of this Node. */
    const Transform &GetLocalTransform() const { return m_local_transform; }

    /*! @returns The world-space translation, scale, rotation of this Node. Influenced by accumulative transformation of all ancestor Nodes. */
    const Transform &GetWorldTransform() const { return m_world_transform; }
    
    /*! @returns The local-space translation of this Node. */
    inline const Vector3 &GetLocalTranslation() const
        { return m_local_transform.GetTranslation(); }
    
    /*! \brief Set the local-space translation of this Node (not influenced by the parent Node) */
    inline void SetLocalTranslation(const Vector3 &translation)
        { SetLocalTransform({translation, m_local_transform.GetScale(), m_local_transform.GetRotation()}); }
    
    /*! @returns The local-space scale of this Node. */
    inline const Vector3 &GetLocalScale() const
        { return m_local_transform.GetScale(); }
    
    /*! \brief Set the local-space scale of this Node (not influenced by the parent Node) */
    inline void SetLocalScale(const Vector3 &scale)
        { SetLocalTransform({m_local_transform.GetTranslation(), scale, m_local_transform.GetRotation()}); }
    
    /*! @returns The local-space rotation of this Node. */
    inline const Quaternion &GetLocalRotation() const
        { return m_local_transform.GetRotation(); }
    
    /*! \brief Set the local-space rotation of this Node (not influenced by the parent Node) */
    inline void SetLocalRotation(const Quaternion &rotation)
        { SetLocalTransform({m_local_transform.GetTranslation(), m_local_transform.GetScale(), rotation}); }
    
    /*! \brief Called each tick of the logic loop of the game. Updates the Spatial transform to be reflective of the Node's world-space transform. */
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