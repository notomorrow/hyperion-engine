#ifndef OCTREE_H
#define OCTREE_H

#include "math/bounding_box.h"
#include "entity.h"
#include "util/non_owning_ptr.h"
#include "util.h"

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

using OctreeChangeCallback_t = std::function<void(OctreeChangeEvent, const Octree *)>;//void(*)(OctreeChangeEvent, const Octree*);

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
        BoundingBox m_aabb;
        non_owning_ptr<Octree> m_octree;

        Node(int id, const BoundingBox &aabb, non_owning_ptr<Octree> octree = nullptr)
            : m_id(id),
              m_aabb(aabb),
              m_octree(octree)
        {
        }

        Node(const Node &other)
            : m_id(other.m_id),
              m_aabb(other.m_aabb),
              m_octree(other.m_octree)
        {
        }

        bool operator==(const Node &other) const
            { return m_id == other.m_id && m_aabb == other.m_aabb && m_octree == other.m_octree; }
        bool operator!=(const Node &other) const
            { return !operator==(other); }
    };

    Octree(const BoundingBox &aabb, int level = 0)
        : m_aabb(aabb),
          m_parent(nullptr),
          m_is_divided(false),
          m_needs_recalculation(false),
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

    inline void AddCallback(OctreeChangeCallback_t cb) { m_callbacks.push_back(cb); }
    /*inline void RemoveCallback(OctreeChangeCallback_t cb)
    {
        auto it = std::find(m_callbacks.begin(), m_callbacks.end(), cb);

        if (it != m_callbacks.end()) {
            m_callbacks.erase(it);
        }
    }*/

    // call when the transform of a node has updated and we need to
    // slot the node into a better place.
    // this function will first check if the node's current octant
    // still fits, and if not, will find the closest octant that fully contains
    // the node, and then insert it into that octant normally.
    /*void NodeTransformChanged(Entity *entity)
    {
        ex_assert(entity->GetOctree() != nullptr);
        std::cout << "node transform changed " << entity->GetName() << "\n";

        non_owning_ptr<Octree> prev_octree(entity->GetOctree());
        entity->GetOctree()->DispatchEvent(OCTREE_NODE_TRANSFORM_CHANGE);
        return;

        //RemoveNode(entity);
        //InsertNode(non_owning_ptr(entity));
        //prev_octree->Prune();

        //return;

        non_owning_ptr<Octree> oct_iter(entity->GetOctree());

        //entity->GetOctree()->DispatchEvent(OCTREE_REMOVE_NODE);
        //entity->SetOctree(non_owning_ptr<Octree>(nullptr));

        while (oct_iter != nullptr) {
            if (oct_iter->Contains(entity->GetAABB())) {
                break;
            }

            oct_iter = oct_iter->m_parent;
        }
        //RemoveNode(entity, false);
        //ex_assert(oct_iter != entity->GetOctree());
        prev_octree->RemoveNode(entity, false);

        if (oct_iter == nullptr) {
            // no parent can fully contain this node
            std::cout << "no parent can fully contain " << entity->GetName() << "  " << entity->GetAABB() << "\n";
            return;
        }
        std::cout << "insert back into " << oct_iter->GetAABB() << "\n";
        oct_iter->InsertNode(non_owning_ptr(entity));

        // now, undivide?
        prev_octree->Prune();
    }*/

    void Clear()
    {
        /*for (auto &node : m_nodes) {
            if (node == nullptr) {
                continue;
            }

            node->SetOctree(non_owning_ptr<Octree>(nullptr));
        }*/
        
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

    // walk up chain undividing octants until a non-empty octant is found
    void Prune()
    {
        non_owning_ptr<Octree> oct_iter(this);

        //entity->GetOctree()->DispatchEvent(OCTREE_REMOVE_NODE);
        //entity->SetOctree(non_owning_ptr<Octree>(nullptr));

        while (oct_iter != nullptr) {
            if (!oct_iter->m_is_divided) {
                break;
            }

            if (!oct_iter->AllEmpty()) {
                break;
            }

            oct_iter->Undivide();

            oct_iter = oct_iter->m_parent;
        }
    }

    // called on octree A1 with entity with octree A3:
    //    removes entity from A3,
    //      checks if A3 is empty, if so, undivides octants
    //      checks if A2 is empty, if so, undivides octants
    //      stops at A1 (this)
    // called on octree A3 with entity with octree A3:
    //    removes entity from A3
    //    checks if A3 is empty, if so, undivides octants
    bool RemoveNode(int id, bool undivide = true)
    {
        //ex_assert(node.m_octree != nullptr);

        std::cout << " from " << m_level << ":\n";
        std::cout << "=== REMOVING " << id << " ===\n";

        auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [id](const Node &node) {
            return node.m_id == id;
        });

        if (it != m_nodes.end()) {
            m_nodes.erase(it);
            DispatchEvent(OCTREE_REMOVE_NODE);

            if (undivide) {
                std::cout << "STARTING UNDIVIDE FROM " << m_level << "\n";
                std::cout << "  is divided? " << m_is_divided << "\n";

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
            }

            return true;
        }

        if (m_is_divided) {
            for (Octant &octant : m_octants) {
                hard_assert(octant.m_octree != nullptr);

                if (octant.m_octree->RemoveNode(id, undivide)) {
                    return true;
                }
            }
        }

        return false;


        // look through my nodes, see if that entity exists
        // how to handle undivision?
       // non_owning_ptr<Octree> oct_iter(node.m_octree);
        
        /*for (size_t i = 0; i < entity->NumChildren(); i++) {
            if (auto &child = entity->GetChild(i)) {
                RemoveNode(child.get(), undivide);
            }
        }*/

        //oct_iter->m_nodes.erase(it);
        //oct_iter->DispatchEvent(OCTREE_REMOVE_NODE);
        //entity->SetOctree(non_owning_ptr<Octree>(nullptr));

        //std::cout << "Remove " << node.m_aabb << "\n";
        //std::cout << " has " << oct_iter->m_nodes.size() << " children\n";
        //std::cout << "\n\n";

        //std::cout << "=== END " << node.m_aabb << " ===\n";
    }

    void InsertNode(const Node &node, bool dispatch_callbacks = true)
    {
        for (Octant &octant : m_octants) {
            if (octant.Contains(node.m_aabb)) {
                if (!m_is_divided) {
                    Divide();
                }

                hard_assert(octant.m_octree != nullptr);

                octant.m_octree->InsertNode(node);

                return;
            }
        }

        // no child octants can fully contain entity.
        // so we take it
        m_nodes.push_back(node);

        if (dispatch_callbacks) {
            DispatchEvent(OCTREE_INSERT_NODE);
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

    void DispatchEvent(OctreeChangeEvent evt) const
    {
        const Octree *oct = this;

        while (oct != nullptr) {
            for (auto &cb : oct->m_callbacks) {
                cb(evt, this);
            }

            oct = oct->m_parent.get();
        }
    }

    void UnsetEntityOctree(Entity *entity)
    {
        entity->SetOctree(non_owning_ptr<Octree>(nullptr));

        // since each child node would be contained within the parent entity's octant as well,
        // we can just go ahead and set each child's octree to nullptr

        for (size_t i = 0; i < entity->NumChildren(); i++) {
            if (entity->GetChild(i) == nullptr) {
                continue;
            }

            UnsetEntityOctree(entity->GetChild(i).get());
        }
    }

    bool m_needs_recalculation;

    std::vector<Node> m_nodes;

    std::vector<OctreeChangeCallback_t> m_callbacks;
};

} // namespace hyperion

#endif