#ifndef OCTREE_H
#define OCTREE_H

#include "../math/bounding_box.h"
#include "../math/frustum.h"
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
    OCTREE_NODE_TRANSFORM_CHANGE = 16,
    OCTREE_VISIBILITY_STATE = 32
};

class Octree;

using OctreeChangeCallback_t = std::function<void(OctreeChangeEvent, const Octree *, int, const Spatial*, void *)>;


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

    struct OctreeResult {
        enum {
            OCTREE_OK = 0,
            OCTREE_ERR = 1
        } result;

        non_owning_ptr<Octree> octree;
        std::string message;

        OctreeResult(decltype(result) result, non_owning_ptr<Octree> octree)
            : result(result),
              octree(octree)
        {
        }

        OctreeResult(decltype(result) result, const std::string &message, non_owning_ptr<Octree> octree = nullptr)
            : result(result),
              octree(octree),
              message(message)
        {
        }

        OctreeResult(const OctreeResult &other)
            : result(other.result),
              octree(other.octree),
              message(other.message)
        {
        }

        inline operator bool() { return result == OCTREE_OK; }
        inline operator bool() const { return result == OCTREE_OK; }
    };

    struct VisibilityState {
        enum CameraType {
            VIS_CAMERA_NONE,
            VIS_CAMERA_MAIN,
            VIS_CAMERA_SHADOW0,
            VIS_CAMERA_SHADOW1,
            VIS_CAMERA_SHADOW2,
            VIS_CAMERA_SHADOW3,
            VIS_CAMERA_VOXEL0,
            VIS_CAMERA_VOXEL1,
            VIS_CAMERA_VOXEL2,
            VIS_CAMERA_VOXEL3,
            VIS_CAMERA_VOXEL4,
            VIS_CAMERA_VOXEL5,
            VIS_CAMERA_OTHER0,
            VIS_CAMERA_OTHER1,
            VIS_CAMERA_OTHER2,

            VIS_CAMERA_MAX
        };

        // visibility is not determined by a boolean, as we'd have to traverse the whole octree,
        // and doing this every frame when unnecessary is a bit of a performance killer.
        // instead we calculate a hash at the start of the visibility check, and if the octant is visible,
        // tag it with the new hash then once we hit a point of non-visibility, the octants will not be given the new hash.
        // we can then check an octants visibility by comparing the hashes.
        std::array<HashCode::Value_t, VIS_CAMERA_MAX> check_id;

        VisibilityState() : check_id({ 0 }) {}
        VisibilityState(const decltype(check_id) &check_id) : check_id(check_id) {}
        VisibilityState(const VisibilityState &other) : check_id(other.check_id) {}
        inline VisibilityState &operator=(const VisibilityState &other) { check_id = other.check_id; return *this; }
        //inline bool operator==(const VisibilityState &other) const { return check_id == other.check_id; }
        inline bool Compare(CameraType type, const VisibilityState &other) const { return check_id[type] == other.check_id[type]; }
        inline HashCode::Value_t &operator[](CameraType type) { return check_id[type]; }
        inline const HashCode::Value_t &operator[](CameraType type) const { return check_id[type]; }

        ~VisibilityState() = default;
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
        if (m_is_divided) {
            for (Octant &octant : m_octants) {
                hard_assert(octant.m_octree != nullptr);

                delete octant.m_octree.get();
            }
        }

        if (!m_nodes.empty()) {
            for (const auto &node : m_nodes) {
                DispatchEvent(OCTREE_REMOVE_NODE, node.m_id, &node.m_spatial);
            }
        }

        DispatchEvent(OCTREE_REMOVE_OCTANT);
    }

    inline const BoundingBox &GetAABB() const { return m_aabb; }
    inline bool Contains(const BoundingBox &aabb) const { return m_aabb.Contains(aabb); }
    inline bool IsDivided() const { return m_is_divided; }
    inline std::array<Octant, 8> &GetOctants() { return m_octants; }
    inline const std::array<Octant, 8> &GetOctants() const { return m_octants; }
    inline std::vector<Node> &GetNodes() { return m_nodes; }
    inline const std::vector<Node> &GetNodes() const { return m_nodes; }
    inline bool Empty() const { return m_nodes.empty(); }

    inline const VisibilityState &GetVisibilityState() const { return m_visibility_state; }
    inline bool VisibleTo(VisibilityState::CameraType type, const VisibilityState &parent_state) const
        { return m_visibility_state.Compare(type, parent_state); }
    inline bool VisibleToParent(VisibilityState::CameraType type) const
        { return m_parent == nullptr || VisibleTo(type, m_parent->m_visibility_state); }

    /*
     * Note: this octant is assumed to be visible.
     * This is to be called, generally, from the root node of the octree.
     */
    void UpdateVisibilityState(VisibilityState::CameraType type, const Frustum & frustum)
    {
        m_visibility_state[type]++;

        UpdateVisibilityState(frustum, type, m_visibility_state);
    }

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

    OctreeResult UpdateNode(int id, const Spatial &new_value)
    {
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [id](const Node &node) {
            return node.m_id == id;
        });

        if (it == m_nodes.end()) {
            if (m_is_divided) {
                for (Octant &octant : m_octants) {
                    hard_assert(octant.m_octree != nullptr);

                    if (auto result = octant.m_octree->UpdateNode(id, new_value)) {
                        return result;
                    }
                }
            }

            return OctreeResult(OctreeResult::OCTREE_ERR, "Spatial not found in this nor any child octants");
        }

        if (new_value.GetAABB() == it->m_spatial.GetAABB()) {
            m_nodes[it - m_nodes.begin()].m_spatial = new_value;

            // DispatchEvent(OCTREE_NODE_TRANSFORM_CHANGE, new_node.m_id, &new_node.m_spatial);

            return OctreeResult(OctreeResult::OCTREE_OK, "AABB has not changed, no need to update octree", non_owning_ptr(this));
        }

        DispatchEvent(OCTREE_REMOVE_NODE, it->m_id, &it->m_spatial);
        m_nodes.erase(it);

        // aabb has changed - have to remove it from this octant and look up chain
        // for a higher level that contains the new aabb
        non_owning_ptr<Octree> new_parent(m_parent);

        if (new_parent == nullptr || Contains(new_value.GetAABB())) {
            // top-level: try insert bounding box into new subdivision
            auto insert_result = InsertNode(id, new_value);

            if (insert_result) {
                DispatchEvent(OCTREE_NODE_TRANSFORM_CHANGE, id, &new_value);
            }

            return insert_result;

            //return { OctreeResult::OCTREE_OK, "Still contains spatial AABB or at topmost level -- reinserting node", non_owning_ptr(this) };
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

        if (inserted) {
            return OctreeResult(OctreeResult::OCTREE_OK, new_parent);
        }

        return OctreeResult(OctreeResult::OCTREE_ERR, "Object was not inserted -- no suitable parent octant found");
    }

    OctreeResult RemoveNode(int id)
    {
        auto it = std::find_if(m_nodes.begin(), m_nodes.end(), [id](const Node &node) {
            return node.m_id == id;
        });

        if (it != m_nodes.end()) {
            DispatchEvent(OCTREE_REMOVE_NODE, it->m_id, &it->m_spatial);

            m_nodes.erase(it);

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

            return OctreeResult(OctreeResult::OCTREE_OK, non_owning_ptr<Octree>(nullptr));
        }

        if (m_is_divided) {
            for (Octant &octant : m_octants) {
                hard_assert(octant.m_octree != nullptr);

                if (auto result = octant.m_octree->RemoveNode(id)) {
                    return result;
                }
            }
        }

        return OctreeResult(OctreeResult::OCTREE_ERR, "Node could not be removed");
    }

    OctreeResult InsertNode(int id, const Spatial &spatial, bool dispatch_callbacks = true)
    {
        for (Octant &octant : m_octants) {
            if (octant.Contains(spatial.GetAABB())) {
                if (!m_is_divided) {
                    Divide();
                }

                hard_assert(octant.m_octree != nullptr);

                return octant.m_octree->InsertNode(id, spatial);
            }
        }

        // no child octants can fully contain node.
        // so we take it
        m_nodes.push_back(Node{ id, spatial });

        if (dispatch_callbacks) {
            DispatchEvent(OCTREE_INSERT_NODE, id, &spatial);
        }

        return OctreeResult(OctreeResult::OCTREE_OK, non_owning_ptr(this));
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

    void UpdateVisibilityState(const Frustum &frustum, VisibilityState::CameraType type, const VisibilityState &visibility_state)
    {
        m_visibility_state[type] = visibility_state[type];

        if (m_is_divided) {
            for (auto &octant : m_octants) {
                if (!frustum.BoundingBoxInFrustum(octant.m_aabb)) {
                    continue;
                }

                octant.m_octree->UpdateVisibilityState(frustum, type, visibility_state);
            }
        }

        DispatchEvent(OCTREE_VISIBILITY_STATE, -1, nullptr, (void*)int32_t(type));
    }

    void DispatchEvent(OctreeChangeEvent evt, int node_id = -1, const Spatial *spatial = nullptr, void *raw_data = nullptr) const
    {
        const Octree *oct = this;

        while (oct != nullptr) {
            for (auto &cb : oct->m_callbacks) {
                cb(evt, this, node_id, spatial, raw_data);
            }

            oct = oct->m_parent.get();
        }
    }

    std::vector<Node> m_nodes;
    std::vector<OctreeChangeCallback_t> m_callbacks;
    VisibilityState m_visibility_state;
};

} // namespace hyperion

#endif