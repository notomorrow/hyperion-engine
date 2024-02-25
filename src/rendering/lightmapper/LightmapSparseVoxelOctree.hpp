#ifndef HYPERION_V2_LIGHTMAP_SPARSE_VOXEL_OCTREE_HPP
#define HYPERION_V2_LIGHTMAP_SPARSE_VOXEL_OCTREE_HPP

#include <core/Containers.hpp>
#include <core/lib/Pair.hpp>
#include <core/lib/HashMap.hpp>
#include <scene/VisibilityState.hpp>
#include <scene/Octree.hpp> // for OctantID
#include <util/ByteUtil.hpp>

#include <math/Vector3.hpp>
#include <math/BoundingBox.hpp>
#include <math/Ray.hpp>

#include <Types.hpp>

// #define HYP_OCTREE_DEBUG 1

namespace hyperion::v2 {

class LightmapOctree;

struct LightmapOctreeState
{
    // If any octants need to be rebuilt, their topmost parent that needs to be rebuilt will be stored here
    OctantID    rebuild_state = OctantID::invalid;
};

struct LightmapOctreeEntry
{
    Vec4f   color;
    Vec3f   hitpoint;
    uint    triangle_index = ~0u;
};

class LightmapOctree
{
    enum
    {
        DEPTH_SEARCH_INF = -1,
        DEPTH_SEARCH_ONLY_THIS = 0
    };

    static constexpr float growth_factor = 1.5f;
    // the length value at which to stop recusively dividing
    // for a small enough object
    static constexpr float min_aabb_size = 1.0f; 
    static constexpr float texel_size = 1.0f;
    static const BoundingBox default_bounds;

    LightmapOctree(const BoundingBox &aabb, LightmapOctree *parent, uint8 index);

public:
    struct Result
    {
        enum
        {
            OCTREE_OK  = 0,
            OCTREE_ERR = 1
        } result;

        const char  *message;
        int         error_code = 0;

        Result()
            : Result(OCTREE_OK) {}
        
        Result(decltype(result) result, const char *message = "", int error_code = 0)
            : result(result), message(message), error_code(error_code) {}
        Result(const Result &other)
            : result(other.result), message(other.message), error_code(other.error_code) {}

        HYP_FORCE_INLINE
        operator bool() const { return result == OCTREE_OK; }
    };

    using InsertResult = Pair<Result, OctantID>;

    struct Octant
    {
        UniquePtr<LightmapOctree>   octree;
        BoundingBox                 aabb;

        Octant()
            : octree(nullptr), aabb() {}

        Octant(UniquePtr<LightmapOctree> octree, const BoundingBox &aabb)
            : octree(std::move(octree)), aabb(aabb) {}

        Octant(const Octant &other) = delete;
        Octant &operator=(const Octant &other) = delete;

        Octant(Octant &&other) noexcept
            : octree(std::move(other.octree)), aabb(std::move(other.aabb)) {}

        Octant &operator=(Octant &&other) noexcept
        {
            octree = std::move(other.octree);
            aabb = std::move(other.aabb);

            return *this;
        }

        ~Octant() = default;
    };

    LightmapOctree(const BoundingBox &aabb = default_bounds);
    LightmapOctree(const LightmapOctree &other)             = delete;
    LightmapOctree &operator=(const LightmapOctree &other)  = delete;
    LightmapOctree(LightmapOctree &&other) noexcept;
    LightmapOctree &operator=(LightmapOctree &&other) noexcept;
    ~LightmapOctree();

    bool IsRoot() const { return m_parent == nullptr; }
    bool IsBottomLevel() const { return !m_is_divided; }

    BoundingBox &GetAABB()
        { return m_aabb; }

    const BoundingBox &GetAABB() const
        { return m_aabb; }

    const LightmapOctreeEntry &GetValue() const
        { return m_entry; }

    OctantID GetOctantID() const
        { return m_octant_id; }

    const FixedArray<Octant, 8> &GetOctants() const
        { return m_octants; }

    /*! \brief Get the child (nested) octant with the specified index
     *  \param octant_id The OctantID to use to find the octant (see OctantID struct)
     *  \return The octant with the specified index, or nullptr if it doesn't exist
    */
    LightmapOctree *GetChildOctant(OctantID octant_id);

    bool IsDivided() const
        { return m_is_divided; }
        
    void Clear();
    InsertResult Insert(LightmapOctreeEntry entry);
    InsertResult Rebuild();
    InsertResult Rebuild(const BoundingBox &new_aabb);

    LightmapOctreeState *GetState() const
        { return m_state; }

    template <class Lambda>
    void ForEachOctant(Lambda &&lambda) const
    {
        if (IsBottomLevel()) {
            lambda(this);
        } else {
            for (const Octant &octant : m_octants) {
                AssertThrow(octant.octree != nullptr);

                octant.octree->ForEachOctant(lambda);
            }
        }
    }

private:
    /*! \brief Used for parent octants to store an averaged result of all children */
    void RebuildEntry(uint depth = 0);

    void ClearInternal(Array<LightmapOctreeEntry> &out_entries);
    void Clear(Array<LightmapOctreeEntry> &out_entries);
    
    void SetParent(LightmapOctree *parent);

    void InitOctants();
    void Divide();
    void Undivide();

    InsertResult InsertInternal(LightmapOctreeEntry entry);
    InsertResult RebuildExtendInternal(Vec3f extend_include_point);


    LightmapOctree                  *m_parent;
    BoundingBox                     m_aabb;
    FixedArray<Octant, 8>           m_octants;
    bool                            m_is_divided;
    LightmapOctreeState             *m_state;
    OctantID                        m_octant_id;

    LightmapOctreeEntry             m_entry;
};

} // namespace hyperion::v2

#endif
