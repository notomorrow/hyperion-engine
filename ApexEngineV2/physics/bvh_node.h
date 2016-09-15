#ifndef BVH_NODE_H
#define BVH_NODE_H

#include "contact.h"
#include "rigid_body.h"

#include <vector>
#include <array>

namespace apex {
template <typename BoundingVolumeClass>
class BVHNode {
public:
    BVHNode(BVHNode *parent, const BoundingVolumeClass &volume, RigidBodyControl *body)
        : m_parent(parent), m_volume(volume), m_body(body)
    {
        m_children = { nullptr, nullptr };
    }

    ~BVHNode()
    {
        if (m_parent != nullptr) {
            BVHNode *sibling;
            if (m_parent->m_children[0] == this) {
                sibling = m_parent->m_children[1];
            } else {
                sibling = m_parent->m_children[0];
            }

            m_parent->m_volume = sibling->m_volume;
            m_parent->m_body = sibling->m_body;
            m_parent->m_children = sibling->m_children;

            sibling->m_parent = nullptr;
            sibling->m_body = nullptr;
            sibling->m_children = { nullptr, nullptr };
            delete sibling;

            m_parent->RecalculateBoundingVolume();
        }
    }

    bool IsLeaf() const
    {
        return m_body != nullptr;
    }

    bool Overlaps(const BVHNode &other) const
    {
        return m_volume.Overlaps(other.m_volume);
    }

    void RecalculateBoundingVolume(bool recursive = true)
    {
        if (IsLeaf()) {
            return;
        }

        m_volume = BoundingVolumeClass(m_children[0]->m_volume, 
            m_children[1]->m_volume);

        if (recursive && m_parent != nullptr) {
            m_parent->RecalculateBoundingVolume(true);
        }
    }

    unsigned int GetPotentialContactsWith(const BVHNode *other, std::vector<PotentialContact> &contacts, 
        unsigned int index, unsigned int limit) const
    {
        if (!Overlaps(other) || limit == 0) {
            return 0;
        }

        if (IsLeaf() && other.IsLeaf()) {
            contacts[index].m_bodies[0] = m_body;
            contacts[index].m_bodies[1] = other.m_body;
            return 1;
        }

        if (other.IsLeaf() || (!IsLeaf() && m_volume.GetVolume() >= other->m_volume.GetVolume())) {
            unsigned int count = m_children[0]->GetPotentialContactsWith(other, contacts, index, limit);

            if (limit > count) {
                return count + m_children[1]->GetPotentialContactsWith(other, contacts, index + count, limit - count);
            } else {
                return count;
            }
        } else {
            unsigned int count = GetPotentialContactsWith(other->m_children[0], contacts, index, limit);

            if (limit > count) {
                return count + GetPotentialContactsWith(other->m_children[1], contacts, index + count, limit - count);
            } else {
                return count;
            }
        }
    }

    unsigned int GetPotentialContacts(std::vector<PotentialContact> &contacts, unsigned int limit) const
    {
        if (IsLeaf()) {
            return 0;
        }

        return m_children[0]->GetPotentialContactsWith(m_children[1], contacts, 0, limit);
    }

protected:
    std::array<BVHNode*, 2> m_children;
    BoundingVolumeClass m_volume;
    RigidBodyControl *m_body;
    BVHNode *m_parent;
};
}

#endif