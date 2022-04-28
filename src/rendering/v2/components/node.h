#ifndef HYPERION_V2_NODE_H
#define HYPERION_V2_NODE_H

#include "spatial.h"
#include "containers.h"
#include "mixins.h"
#include <math/transform.h>

#include <vector>
#include <memory>

namespace hyperion::v2 {

class Engine;

class Node : public mixins::HasAccelerationStructure<Node> {
public:
    using NodeList = std::vector<std::unique_ptr<Node>>;

    enum class Type {
        NODE,
        BONE
    };

    enum Flags {
        NODE_FLAGS_NONE = 0
    };

    /*! \brief Construct the node, optionally taking in a string tag to improve identification.
     * @param tag A c-string representing the name of the Node. The memory is copied internally so the string can be safely deleted
     * after use.
     * @param local_transform An optional parameter representing the local-space transform of this Node.
     */
    Node(
        const char *tag = "",
        const Transform &local_transform = Transform()
    );

    Node(
        const char *tag,
        Ref<Spatial> &&spatial,
        const Transform &local_transform = Transform()
    );

    Node(const Node &other) = delete;
    Node &operator=(const Node &other) = delete;
    ~Node();

    /*! @returns The string tag that was given to the Node on creation. */
    const char *GetTag() const { return m_tag; }
    /*! @returns The type of the node. By default, it will just be NODE. */
    Type GetType() const { return m_type; }
    /*! @returns A pointer to the parent Node of this Node. May be null. */
    inline Node *GetParent() const { return m_parent_node; }

    inline Spatial *GetSpatial() const { return m_spatial.ptr; }
    void SetSpatial(Ref<Spatial> &&spatial);

    /*! \brief Add the Node as a child of this object, taking ownership over the given Node.
     * @param node The Node to be added as achild of this Node
     */
    void AddChild(std::unique_ptr<Node> &&node);

    /*! \brief Remove a child using the given iterator (i.e from FindChild())
     * @param iter The iterator from this Node's child list
     * @returns Whether then removal was successful
     */
    bool RemoveChild(NodeList::iterator iter);

    /*! \brief Remove a child at the given index
     * @param index The index of the child element to remove
     * @returns Whether then removal was successful
     */
    bool RemoveChild(size_t index);

    /*! \brief Remove this node from the parent Node's list of child Nodes. */
    bool Remove();
    
    /*! \brief Get a child Node from this Node's child list at the given index.
     * @param index The index of the child element to return
     * @returns The child node at the given index. If the index is out of bounds, nullptr
     * will be returned.
     */
    Node *GetChild(size_t index) const;

    /*! \brief Get an iterator for the given child Node from this Node's child list
     * @param index The node to find in this Node's child list
     * @returns The resulting iterator
     */
    NodeList::iterator FindChild(Node *node);

    inline NodeList &GetChildren() { return m_child_nodes; }
    inline const NodeList &GetChildren() const { return m_child_nodes; }

    /*! \brief Get all descendent child Nodes from this Node. This vector is pre-calculated,
     * so no calculation happens when calling this method.
     * @returns A vector of raw pointers to descendent Nodes
     */
    inline const std::vector<Node *> &GetDescendents() const { return m_descendents; }

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

    /*! \brief Move the Node in local-space by adding the given vector to the current local-space translation.
     * @param translation The vector to translate this Node by
     */
    inline void Translate(const Vector3 &translation)
        { SetLocalTranslation(m_local_transform.GetTranslation() + translation); }
    
    /*! @returns The local-space scale of this Node. */
    inline const Vector3 &GetLocalScale() const
        { return m_local_transform.GetScale(); }
    
    /*! \brief Set the local-space scale of this Node (not influenced by the parent Node) */
    inline void SetLocalScale(const Vector3 &scale)
        { SetLocalTransform({m_local_transform.GetTranslation(), scale, m_local_transform.GetRotation()}); }

    /*! \brief Scale the Node in local-space by multiplying the current local-space scale by the given scale vector.
     * @param scale The vector to scale this Node by
     */
    inline void Scale(const Vector3 &scale)
        { SetLocalScale(m_local_transform.GetScale() * scale); }
    
    /*! @returns The local-space rotation of this Node. */
    inline const Quaternion &GetLocalRotation() const
        { return m_local_transform.GetRotation(); }
    
    /*! \brief Set the local-space rotation of this Node (not influenced by the parent Node) */
    inline void SetLocalRotation(const Quaternion &rotation)
        { SetLocalTransform({m_local_transform.GetTranslation(), m_local_transform.GetScale(), rotation}); }

    /*! \brief Rotate the Node by multiplying the current local-space rotation by the given quaternion.
     * @param rotation The quaternion to rotate this Node by
     */
    inline void Rotate(const Quaternion &rotation)
        { SetLocalRotation(m_local_transform.GetRotation() * rotation); }

    /*! @returns The local-space (model) of the node's aabb. Only includes
     * the Spatial's aabb.
     */
    inline const BoundingBox &GetLocalAabb() const { return m_local_aabb; }

    /*! @returns The world-space aabb of the node. Includes the transforms of all
     * parent nodes.
     */
    inline const BoundingBox &GetWorldAabb() const { return m_world_aabb; }

    
    void UpdateWorldTransform();
    /*! \brief Called each tick of the logic loop of the game. Updates the Spatial transform to be reflective of the Node's world-space transform. */
    void Update(Engine *engine);
    
    void CreateAccelerationStructure(Engine *engine);
    void DestroyAccelerationStructure(Engine *engine);

protected:
    Node(
        Type type,
        const char *tag,
        Ref<Spatial> &&spatial,
        const Transform &local_transform = Transform()
    );

    void UpdateInternal(Engine *engine);
    void OnNestedNodeAdded(Node *node);
    void OnNestedNodeRemoved(Node *node);

    AccelerationGeometry *GetOrCreateSpatialAccelerationGeometry(Engine *engine);

    Type m_type = Type::NODE;
    char *m_tag;
    Node *m_parent_node;
    NodeList m_child_nodes;
    Transform m_local_transform;
    Transform m_world_transform;
    BoundingBox m_local_aabb;
    BoundingBox m_world_aabb;

    Ref<Spatial> m_spatial;

    std::vector<Node *> m_descendents;
};

} // namespace hyperion::v2

#endif