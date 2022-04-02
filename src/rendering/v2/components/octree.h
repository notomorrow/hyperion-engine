#ifndef HYPERION_V2_OCTREE_H
#define HYPERION_V2_OCTREE_H

#include "spatial.h"
#include "util.h"

#include <math/vector3.h>
#include <math/bounding_box.h>

namespace hyperion::v2 {

class Octree;

struct OctreeCallback {
    using CallbackFunction = std::function<void(Engine *, Octree *, Spatial *)>;
};

struct OctreeEvents : ComponentEvents<OctreeCallback> {
    Callbacks on_insert_octant, on_remove_octant, on_insert_node, on_remove_node;
};

class Octree {
    friend class Engine;
public:
    struct Octant {
        std::unique_ptr<Octree> octree; /* non-owning */
        BoundingBox aabb;
    };

    struct Node {
        Spatial *spatial;
        BoundingBox aabb;
    };

    struct Root {
        OctreeEvents events;
        std::unordered_map<Spatial *, Octree *> node_to_octree;
    };

    Octree(const BoundingBox &aabb)
        : m_aabb(aabb),
          m_parent(nullptr),
          m_is_divided(false),
          m_root(nullptr)
    {
        InitOctants();
    }

    ~Octree();

    inline Root *GetRoot() const { return m_root; }

    inline BoundingBox &GetAabb() { return m_aabb; }
    inline const BoundingBox &GetAabb() const { return m_aabb; }

    inline OctreeEvents &GetEvents()
    {
        AssertThrow(m_root != nullptr);

        return m_root->events;
    }

    inline const OctreeEvents &GetEvents() const
        { return const_cast<Octree *>(this)->GetEvents(); }

    void Clear(Engine *engine);
    bool Insert(Engine *engine, Spatial *spatial);
    bool Remove(Engine *engine, Spatial *spatial);
    bool Update(Engine *engine, Spatial *spatial);

private:
    inline const auto FindNode(Spatial *spatial)
    {
        return std::find_if(m_nodes.begin(), m_nodes.end(), [spatial](const Node &node) {
            return node.spatial == spatial;
        });
    }

    inline bool IsRoot() const { return m_parent == nullptr; }

    void SetParent(Octree *parent);

    void InitOctants();
    void Divide(Engine *engine);
    void Undivide(Engine *engine);
    bool InsertInternal(Engine *engine, Spatial *spatial);
    bool UpdateInternal(Engine *engine, Spatial *spatial);
    bool RemoveInternal(Engine *engine, Spatial *spatial);

    std::vector<Node> m_nodes;
    Octree *m_parent;
    BoundingBox m_aabb;
    std::array<Octant, 8> m_octants;
    bool m_is_divided;
    Root *m_root;
};

} // namespace hyperion::v2

#endif