#ifndef OCTREE_H
#define OCTREE_H

#include "../math/bounding_box.h"
#include "spatial.h"
#include "../util/non_owning_ptr.h"
#include "../util.h"

#include <array>
#include <functional>
#include <unordered_map>

namespace hyperion {

enum OctreeChangeEvent {
    OCTREE_NONE = 0,
    OCTREE_INSERT_OCTANT = 1,
    OCTREE_REMOVE_OCTANT = 2,
    OCTREE_INSERT_NODE = 4,
    OCTREE_REMOVE_NODE = 8,
    OCTREE_NODE_TRANSFORM_CHANGE = 16
};

class Octree;

using OctreeChangeCallback_t = std::function<void(OctreeChangeEvent, const Octree *, int, const Spatial*)>;

class Octree {
public:
    friend std::ostream &operator<<(std::ostream &out, const Octree &octree);

    struct Octant {
        non_owning_ptr<Octree> m_octree;
        BoundingBox m_aabb;

        Octant() : m_aabb(BoundingBox()), m_octree(nullptr) {}

        Octant(const BoundingBox &aabb)
            : m_aabb(aabb),
              m_octree(nullptr)
        {
        }

        Octant(const Octant &other)
            : m_octree(other.m_octree),
              m_aabb(other.m_aabb)
        {
        }

        inline bool Contains(const BoundingBox &aabb) const { return m_aabb.Contains(aabb); }
    };

    struct Node {
        int m_id;
        Spatial m_spatial;

        Node(int id, const Spatial &spatial)
            : m_id(id),
              m_spatial(spatial)
        {
        }

        Node(const Node &other)
            : m_id(other.m_id),
              m_spatial(other.m_spatial)
        {
        }

        bool operator==(const Node &other) const
            { return m_id == other.m_id && m_spatial == other.m_spatial; }
        bool operator!=(const Node &other) const
            { return !operator==(other); }
    };

    Octree(const BoundingBox &aabb, int level = 0)
        : m_aabb(aabb),
          m_parent(nullptr),
          m_is_divided(false),
          m_level(level)
    {
        Vector3 divided_aabb_dimensions = m_aabb.GetDimensions() / 2;

        for (int x = 0; x < 2; x++) {
            for (int y = 0; y < 2; y++) {
                for (int z = 0; z < 2; z++) {
                    int index = 4 * x + 2 * y + z;

                    Octant octant(BoundingBox(
                        m_aabb.GetMin() + (divided_aabb_dimensions * Vector3(x, y, z)),
                        m_aabb.GetMin() + (divided_aabb_dimensions * (Vector3(x, y, z) + Vector3(1.0f)))
                    ));

                    m_octants[index] = octant;
                }
            }
        }
    }

    Octree(const Octree &other) = delete;
    Octree &operator=(const Octree &other) = delete;

    ~Octree()
    {
        DispatchEvent(OCTREE_REMOVE_OCTANT);

        if (m_is_divided) {
            for (Octant &octant : m_octants) {
                hard_assert(octant.m_octree != nullptr);

                delete octant.m_octree.get();
            }
        }


        /*for (auto &node : m_nodes) {
            if (node == nullptr) {
                continue;
            }

            node->SetOctree(non_owning_ptr<Octree>(nullptr));
        }*/
    }

    inline const BoundingBox &GetAABB() const { return m_aabb; }
    inline bool Contains(const BoundingBox &aabb) const { return m_aabb.Contains(aabb); }
    inline bool IsDivided() const { return m_is_divided; }
    inline std::array<Octant, 8> &GetOctants() { return m_octants; }
    inline const std::array<Octant, 8> &GetOctants() const { return m_octants; }
    inline std::vector<Node> &GetNodes() { return m_nodes; }
    inline const std::vector<Node> &GetNodes() const { return m_nodes; }
    inline bool Empty() const { return m_nodes.empty(); }

    bool AllEmpty() const
    {
        if (!m_is_divided) {
            return Empty();
        }

        for (int i = 0; i < m_octants.size(); i++) {
            hard_assert(m_octants[i].m_octree != nullptr);

            if (!m_octants[i].m_octree->AllEmpty()) {
                return false;
            }
        }

        return true;
    }

    inline int AddCallback(OctreeChangeCallback_t cb)
    {
        int callback_id = m_callbacks.size();

        m_callbacks.push_back(cb);

        return callback_id;
    }

    inline void RemoveCallback(int callback_id)
    {
        if (callback_id >= m_callbacks.size()) {
            return;
        }

        m_callbacks.erase(m_callbacks.begin() + callback_id);
    }

    void Clear()
    {
        m_nodes.clear();

        if (m_is_divided) {
            for (size_t i = 0; i < m_octants.size(); i++) {
                hard_assert(m_octants[i].m_octree != nullptr);

                m_octants[i].m_octree->Clear();
                m_octants[i].m_octree->DispatchEvent(OCTREE_REMOVE_OCTANT);

                delete m_octants[i].m_octree.get();
                m_octants[i].m_octree = nullptr;
            }

            m_is_divided = false;
        }
    }

    bool UpdateNode(int id, const Spatial &new_value)
    {
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [id](const Node &node) {
            return node.m_id == id;
        });

        if (it == m_nodes.end()) {
            if (m_is_divided) {
                for (Octant &octant : m_octants) {
                    hard_assert(octant.m_octree != nullptr);

                    if (octant.m_octree->UpdateNode(id, new_value)) {
                        return true;
                    }
                }
            }

            return false;
        }

        if (new_value.GetAABB() == it->m_spatial.GetAABB()) {
            m_nodes[it - m_nodes.begin()].m_spatial = new_value;

            // DispatchEvent(OCTREE_NODE_TRANSFORM_CHANGE, new_node.m_id, &new_node.m_spatial);

            return true;
        }

        DispatchEvent(OCTREE_REMOVE_NODE, it->m_id, &it->m_spatial);
        m_nodes.erase(it);

        // aabb has changed - have to remove it from this octant and look up chain
        // for a higher level that contains the new aabb
        non_owning_ptr<Octree> new_parent(m_parent);

        if (new_parent == nullptr || Contains(new_value.GetAABB())) {
            // top-level: try insert bounding box into new subdivision
            InsertNode(id, new_value);
            DispatchEvent(OCTREE_NODE_TRANSFORM_CHANGE, id, &new_value);

            return true;
        }

        // Contains() is false at this point
        // new_parent is not nullptr at this point

        bool inserted = false;

        do {
            if (new_parent->Contains(new_value.GetAABB())) {
                break;
            }

            new_parent = new_parent->m_parent;
        } while (new_parent != nullptr);

        if (new_parent != nullptr) {
            new_parent->m_nodes.push_back(Node{ id, new_value });
            inserted = true;

            new_parent->DispatchEvent(OCTREE_INSERT_NODE, id, &new_value);

            DispatchEvent(OCTREE_NODE_TRANSFORM_CHANGE, id, &new_value);
        }

        // ok, added node to that (or this, potentially) octant
        // now to remove any empty octants left over
        if (!m_is_divided) {
            if (Empty()) {
                non_owning_ptr<Octree> oct_iter(m_parent);

                while (oct_iter != nullptr && oct_iter->AllEmpty()) {
                    hard_assert_msg(oct_iter->m_is_divided, "Should not have undivided octants throughout the octree");

                    oct_iter->Undivide();

                    oct_iter = oct_iter->m_parent;
                }
            }
        }

        return inserted;
    }

    bool RemoveNode(int id)
    {
        //ex_assert(node.m_octree != nullptr);

        //std::cout << " from " << m_level << ":\n";
        //std::cout << "=== REMOVING " << id << " ===\n";

        auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [id](const Node &node) {
            return node.m_id == id;
        });

        if (it != m_nodes.end()) {
            DispatchEvent(OCTREE_REMOVE_NODE, it->m_id, &it->m_spatial);
            m_nodes.erase(it);

            if (!m_is_divided) {

                //if (m_is_divided && AllEmpty()) {
                //    Undivide();
                //}

                if (Empty()) {
                    non_owning_ptr<Octree> oct_iter(m_parent);

                    while (oct_iter != nullptr && oct_iter->AllEmpty()) {
                        std::cout << "Undividing    " << oct_iter->m_level << "\n";
                        hard_assert_msg(oct_iter->m_is_divided, "Should not have undivided octants throughout the octree");

                        oct_iter->Undivide();

                        oct_iter = oct_iter->m_parent;
                    }
                }
            }

            return true;
        }

        if (m_is_divided) {
            for (Octant &octant : m_octants) {
                hard_assert(octant.m_octree != nullptr);

                if (octant.m_octree->RemoveNode(id)) {
                    return true;
                }
            }
        }

        return false;
    }

    void InsertNode(int id, const Spatial &spatial, bool dispatch_callbacks = true)
    {
        for (Octant &octant : m_octants) {
            if (octant.Contains(spatial.GetAABB())) {
                if (!m_is_divided) {
                    Divide();
                }

                hard_assert(octant.m_octree != nullptr);

                octant.m_octree->InsertNode(id, spatial);

                return;
            }
        }

        // no child octants can fully contain node.
        // so we take it
        m_nodes.push_back(Node{ id, spatial });

        if (dispatch_callbacks) {
            DispatchEvent(OCTREE_INSERT_NODE, id, &spatial);
        }
    }

    void Undivide()
    {
        ex_assert(m_is_divided);

        for (size_t i = 0; i < m_octants.size(); i++) {
            hard_assert(m_octants[i].m_octree != nullptr);

            delete m_octants[i].m_octree.get();
            m_octants[i].m_octree = nullptr;
        }

        m_is_divided = false;
    }

    void Divide()
    {
        ex_assert(!m_is_divided);

        for (Octant &octant : m_octants) {
            hard_assert(octant.m_octree == nullptr);

            octant.m_octree = non_owning_ptr(new Octree(octant.m_aabb, m_level + 1));
            octant.m_octree->m_parent = non_owning_ptr(this);
            octant.m_octree->DispatchEvent(OCTREE_INSERT_OCTANT);
        }

        m_is_divided = true;
    }

    std::array<Octant, 8> m_octants;
    non_owning_ptr<Octree> m_parent;
    BoundingBox m_aabb;
    bool m_is_divided;
    int m_level;

private:

    void DispatchEvent(OctreeChangeEvent evt, int node_id = -1, const Spatial *spatial = nullptr) const
    {
        const Octree *oct = this;

        while (oct != nullptr) {
            for (auto &cb : oct->m_callbacks) {
                cb(evt, this, node_id, spatial);
            }

            oct = oct->m_parent.get();
        }
    }

    std::vector<Node> m_nodes;
    std::vector<OctreeChangeCallback_t> m_callbacks;
};

} // namespace hyperion

#endif