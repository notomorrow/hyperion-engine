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
    Octree(const BoundingBox &aabb)
        : m_aabb(aabb),
          m_parent(nullptr),
          m_octants({ nullptr }),
          m_is_divided(false),
          m_needs_recalculation(false)
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

    inline void AddCallback(OctreeChangeCallback_t cb) { m_callbacks.push_back(cb); }
    /*inline void RemoveCallback(OctreeChangeCallback_t cb)
    {
        auto it = std::find(m_callbacks.begin(), m_callbacks.end(), cb);

        if (it != m_callbacks.end()) {
            m_callbacks.erase(it);
        }
    }*/

    /*void DetectChanges()
    {
        if (m_needs_recalculation) {
            return;
        }

        if (!m_is_divided) {
            return;
        }

        for (const auto &node : m_nodes) {
            if (node->GetFlags() & Entity::UPDATE_TRANSFORM ||
                node->GetFlags() & Entity::UPDATE_AABB ||
                node->GetFlags() & Entity::PENDING_REMOVAL)
            {
                m_needs_recalculation = true;
            }

            if (node->GetFlags() & Entity::PENDING_REMOVAL) {
                auto it = std::find(m_nodes.begin(), m_nodes.end(), node);
                if (it != m_nodes.end()) {
                    m_nodes.erase(it); // remove from octree
                }
            }
        }

        for (Octree *octant : m_octants) {
            if (octant == nullptr) {
                continue;
            }

            octant->DetectChanges();
        }
    }

    void Update()
    {
        if (!m_needs_recalculation) {
            return;
        }



        m_needs_recalculation = false;
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

        entity->GetOctree()->DispatchEvent(OCTREE_NODE_TRANSFORM_CHANGE);

        //if (entity->GetOctree()->Contains(entity->GetAABB())) {
        //    std::cout << "object " <<
        //    return; // OK
        //}
        //RemoveNode(entity);
        //InsertNode(non_owning_ptr(entity));

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
        entity->GetOctree()->RemoveNode(entity, false);

        if (oct_iter == nullptr) {
            // no parent can fully contain this node
            std::cout << "no parent can fully contain " << entity->GetName() << "  " << entity->GetAABB() << "\n";
            return;
        }
        std::cout << "insert back into " << oct_iter->GetAABB() << "\n";
        oct_iter->InsertNode(non_owning_ptr(entity));

        // now, undivide?
    }

    // called on octree A1 with entity with octree A3:
    //    removes entity from A3,
    //      checks if A3 is empty, if so, undivides octants
    //      checks if A2 is empty, if so, undivides octants
    //      stops at A1 (this)
    // called on octree A3 with entity with octree A3:
    //    removes entity from A3
    //    checks if A3 is empty, if so, undivides octants
    non_owning_ptr<Octree> RemoveNode(Entity *entity, bool undivide = true)
    {
        if (entity->GetOctree() == nullptr) {
            return non_owning_ptr<Octree>(nullptr);
        }

        m_needs_recalculation = true;

        // look through my nodes, see if that entity exists
        // how to handle undivision?
        non_owning_ptr<Octree> oct_iter(entity->GetOctree());
        non_owning_ptr<Octree> last_oct(oct_iter);
        bool found = false;



        do {
            last_oct = oct_iter;

            if (!found) {
                auto it = std::find(oct_iter->m_nodes.begin(), oct_iter->m_nodes.end(), non_owning_ptr(entity));
                if (it != oct_iter->m_nodes.end()) {
                    oct_iter->m_nodes.erase(it);
                    found = true;

                    oct_iter->DispatchEvent(OCTREE_REMOVE_NODE);

                    if (!undivide) {
                        break;
                    }
                }
            }

            if (undivide && oct_iter->Empty()) {
                oct_iter->Undivide();
            }

            oct_iter = oct_iter->m_parent;
        } while (oct_iter != nullptr && oct_iter != this);

        if (!found) {
            throw std::runtime_error("object " + entity->GetName() + " not in octree");
        }

        UnsetEntityOctree(entity);

        return non_owning_ptr(last_oct);
    }

    void InsertNode(non_owning_ptr<Entity> entity, bool dispatch_callbacks = true)
    {
        if (entity->GetOctree() != nullptr) {
            // TODO: recalculation
        }
        m_needs_recalculation = true;
        //std::cout << "IN insert new node " << intptr_t(this) << " " << m_aabb << "\n";

        if (!m_is_divided) {
            Divide();
        }

        for (Octree *oct : m_octants) {
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
            if (entity->GetChild(i) == nullptr) {
                continue;
            }

            InsertNode(non_owning_ptr(entity->GetChild(i).get()), false);
        }

        if (dispatch_callbacks) {
            DispatchEvent(OCTREE_INSERT_NODE);
        }
    }

    void Undivide()
    {
        ex_assert(Empty()); // TODO

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
                        ));

                        octant->m_parent = non_owning_ptr(this);
                        octant->DispatchEvent(OCTREE_INSERT_OCTANT);
                        
                        m_octants[index] = octant;
                    }
                }
            }
        }

        m_is_divided = true;
    }

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

    std::array<Octree*, 8> m_octants;
    non_owning_ptr<Octree> m_parent;
    BoundingBox m_aabb;
    bool m_is_divided;

    bool m_needs_recalculation;

    std::vector<non_owning_ptr<Entity>> m_nodes;

    std::vector<OctreeChangeCallback_t> m_callbacks;
};

} // namespace hyperion

#endif