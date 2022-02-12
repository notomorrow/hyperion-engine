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
    Octree(const BoundingBox &aabb, int level = 0)
        : m_aabb(aabb),
          m_parent(nullptr),
          m_octants({ nullptr }),
          m_is_divided(false),
          m_needs_recalculation(false),
          m_level(level)
    {
    }

    Octree(const Octree &other) = delete;
    Octree &operator=(const Octree &other) = delete;

    ~Octree()
    {
        for (Octree *oct : m_octants) {
            if (oct == nullptr) {
                continue;
            }

            delete oct;
        }

        for (auto &node : m_nodes) {
            if (node == nullptr) {
                continue;
            }

            node->SetOctree(non_owning_ptr<Octree>(nullptr));
        }
    }

    inline const BoundingBox &GetAABB() const { return m_aabb; }
    inline bool Contains(const BoundingBox &aabb) const { return m_aabb.Contains(aabb); }
    inline bool IsDivided() const { return m_is_divided; }
    inline const std::array<Octree*, 8> &GetOctants() const { return m_octants; }
    inline bool Empty() const { return m_nodes.empty(); }

    bool AllEmpty() const
    {
        if (!m_is_divided) {
            return Empty();
        }

        for (int i = 0; i < m_octants.size(); i++) {
            if (!m_octants[i]->Empty()) {
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
    void NodeTransformChanged(Entity *entity)
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
    }

    void Clear()
    {
        for (auto &node : m_nodes) {
            if (node == nullptr) {
                continue;
            }

            node->SetOctree(non_owning_ptr<Octree>(nullptr));
        }
        
        m_nodes.clear();

        if (m_is_divided) {
            for (size_t i = 0; i < m_octants.size(); i++) {
                if (m_octants[i] == nullptr) {
                    continue;
                }

                m_octants[i]->Clear();
                m_octants[i]->DispatchEvent(OCTREE_REMOVE_OCTANT);

                delete m_octants[i];
                m_octants[i] = nullptr;
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
            std::cout << " check prune at level " << oct_iter->m_level << "\n";
            if (!oct_iter->m_is_divided) {
                break;
            }

            for (auto &node : oct_iter->m_nodes) {
                if (node == nullptr) continue;
                std::cout << "has " << node->GetName() << ", ";
            }
            std::cout << "\n";
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
    void RemoveNode(Entity *entity, bool undivide = true)
    {
        if (entity->GetOctree() == nullptr) {
            return;
        }

        std::cout << "=== REMOVING " << entity->GetName() << " ===\n";
        
        m_needs_recalculation = true;

        // look through my nodes, see if that entity exists
        // how to handle undivision?
        non_owning_ptr<Octree> oct_iter(entity->GetOctree());

        auto it = std::find(oct_iter->m_nodes.begin(), oct_iter->m_nodes.end(), non_owning_ptr(entity));

        if (it == oct_iter->m_nodes.end()) {
            throw std::runtime_error("object " + entity->GetName() + " not in octree");
        }
        
        /*for (size_t i = 0; i < entity->NumChildren(); i++) {
            if (auto &child = entity->GetChild(i)) {
                RemoveNode(child.get(), undivide);
            }
        }*/

        oct_iter->m_nodes.erase(it);
        oct_iter->DispatchEvent(OCTREE_REMOVE_NODE);
        entity->SetOctree(non_owning_ptr<Octree>(nullptr));

        if (undivide) {
            std::cout << " <on level " << m_level << ">\n";
            while (oct_iter) {
                std::cout << "Check if will undivide octree level " << oct_iter->m_level << ": " << oct_iter->AllEmpty() << "\n";
                if (!oct_iter->m_is_divided) {
                    std::cout << "NOT DIVIDED\n";
                    break;
                }
                
                std::cout << "OCTREE LEVEL " << oct_iter->m_level << " HAS :\n";
                for (const auto &node : oct_iter->m_nodes) {
                    if (node == nullptr) {
                        continue;
                    }

                    std::cout << node->GetName() << ", ";
                }
                std::cout << "\tnested: ";
                std::cout << "\n\n";
                int counter = 0;
                for (Octree *octant : oct_iter->m_octants) {
                    counter++;
                    if (octant == nullptr) {
                        continue;
                    }
                    std::cout << "\toctant " << counter << ":\n\t\t";
                    for (const auto &node : octant->m_nodes) {
                        if (node == nullptr) {
                            continue;
                        }

                        std::cout << node->GetName() << ", ";
                    }
                    std::cout << "\n";
                }

                std::cout << "\n\n";
                if (/*oct_iter == this ||*/ !oct_iter->AllEmpty()) {
                    break;
                }

                oct_iter->Undivide();

                oct_iter = oct_iter->m_parent;
            }
        }

        //UnsetEntityOctree(entity);

        std::cout << "=== END " << entity->GetName() << " ===\n";
    }

    void InsertNode(non_owning_ptr<Entity> entity, bool dispatch_callbacks = true)
    {
        if (entity->GetOctree() != nullptr) {
            // TODO: recalculation
            return;
        }

        if (!entity->GetAABBAffectsParent()) {
            return;
        }

        m_needs_recalculation = true;
        //std::cout << "IN insert new node " << intptr_t(this) << " " << m_aabb << "\n";

        if (!m_is_divided) {
            Divide();
        }

        //std::cout << "insert node " << entity->GetName() << "   " << entity->GetAABB() << "\n";

        for (Octree *oct : m_octants) {
            //std::cout << " !checking " << oct->GetAABB() << " contains " << entity->GetAABB() << ":  " << oct->Contains(entity->GetAABB()) << "\n";
            if (oct->Contains(entity->GetAABB())) {
                oct->InsertNode(non_owning_ptr(entity));
                return;
            }
        }

        // no child octants can fully contain entity.
        // so we take it
        entity->SetOctree(non_owning_ptr(this));
        m_nodes.push_back(entity);

        // take all child nodes too
        // note that we started by checking the full AABB of the entity,
        // we can simplify this a bit by only starting to add the child nodes
        // at this "found" octant
        for (size_t i = 0; i < entity->NumChildren(); i++) {
            if (auto child = entity->GetChild(i).get()) {
                if (!child->GetAABBAffectsParent()) {
                    continue;
                }

                InsertNode(non_owning_ptr(child), false);
            }
        }

        if (dispatch_callbacks) {
            DispatchEvent(OCTREE_INSERT_NODE);
        }
    }

    void Undivide()
    {
        //ex_assert(AllEmpty()); // TODO

        if (!m_is_divided) {
            return;
        }

        for (size_t i = 0; i < m_octants.size(); i++) {
            if (m_octants[i] == nullptr) {
                continue;
            }

            m_octants[i]->DispatchEvent(OCTREE_REMOVE_OCTANT);

            delete m_octants[i];
            m_octants[i] = nullptr;
        }

        m_is_divided = false;
    }

    void Divide()
    {
        if (m_is_divided) {
            return;
        }

        Vector3 divided_aabb_dimensions = m_aabb.GetDimensions() / 2;

        for (int x = 0; x < 2; x++) {
            for (int y = 0; y < 2; y++) {
                for (int z = 0; z < 2; z++) {
                    int index = 4 * x + 2 * y + z;

                    if (m_octants[index] == nullptr) {
                        Octree *octant = new Octree(BoundingBox(
                            m_aabb.GetMin() + (divided_aabb_dimensions * Vector3(x, y, z)),
                            m_aabb.GetMin() + (divided_aabb_dimensions * (Vector3(x, y, z) + Vector3(1.0f)))
                        ), m_level + 1);

                        octant->m_parent = non_owning_ptr(this);
                        octant->DispatchEvent(OCTREE_INSERT_OCTANT);
                        
                        m_octants[index] = octant;
                    }
                }
            }
        }

        m_is_divided = true;
    }

    std::array<Octree *, 8> m_octants;
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

    std::vector<non_owning_ptr<Entity>> m_nodes;

    std::vector<OctreeChangeCallback_t> m_callbacks;
};

} // namespace hyperion

#endif