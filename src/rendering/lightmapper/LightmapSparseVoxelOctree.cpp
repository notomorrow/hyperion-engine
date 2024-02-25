#include <rendering/lightmapper/LightmapSparseVoxelOctree.hpp>
#include <Engine.hpp>
#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/camera/Camera.hpp>
#include <Threads.hpp>

namespace hyperion::v2 {

const BoundingBox LightmapOctree::default_bounds = BoundingBox({ -1.0f }, { 1.0f });

LightmapOctree::LightmapOctree(const BoundingBox &aabb)
    : LightmapOctree(aabb, nullptr, 0)
{
    m_state = new LightmapOctreeState;
}

LightmapOctree::LightmapOctree(const BoundingBox &aabb, LightmapOctree *parent, uint8 index)
    : m_aabb(aabb),
      m_parent(nullptr),
      m_is_divided(false),
      m_state(nullptr),
      m_octant_id(index, OctantID::invalid)
{
    if (parent != nullptr) {
        SetParent(parent); // call explicitly to set root ptr
    }

    AssertThrow(m_octant_id.GetIndex() == index);

    InitOctants();
}

LightmapOctree::LightmapOctree(LightmapOctree &&other) noexcept
    : m_aabb(std::move(other.m_aabb)),
      m_parent(other.m_parent),
      m_is_divided(other.m_is_divided),
      m_state(other.m_state),
      m_octant_id(std::move(other.m_octant_id)),
      m_entry(std::move(other.m_entry)),
      m_octants(std::move(other.m_octants))
{
    other.m_parent = nullptr;
    other.m_state = nullptr;
    other.m_is_divided = false;
    other.m_octant_id = OctantID::invalid;

    for (Octant &octant : m_octants) {
        if (octant.octree) {
            octant.octree->SetParent(this);
        }
    }
}

LightmapOctree &LightmapOctree::operator=(LightmapOctree &&other) noexcept
{
    if (std::addressof(other) == this) {
        return *this;
    }

    m_aabb = std::move(other.m_aabb);
    m_parent = other.m_parent;
    m_is_divided = other.m_is_divided;
    m_state = other.m_state;
    m_octant_id = std::move(other.m_octant_id);
    m_entry = std::move(other.m_entry);
    m_octants = std::move(other.m_octants);

    other.m_parent = nullptr;
    other.m_state = nullptr;
    other.m_is_divided = false;
    other.m_octant_id = OctantID::invalid;

    for (Octant &octant : m_octants) {
        if (octant.octree) {
            octant.octree->SetParent(this);
        }
    }

    return *this;
}

LightmapOctree::~LightmapOctree()
{
    Clear();

    if (IsRoot()) {
        delete m_state;
    }
}

void LightmapOctree::SetParent(LightmapOctree *parent)
{
    m_parent = parent;

    if (m_parent != nullptr) {
        m_state = m_parent->m_state;
    } else {
        m_state = nullptr;
    }

    m_octant_id = OctantID(m_octant_id.GetIndex(), parent != nullptr ? parent->m_octant_id : OctantID::invalid);


    if (IsDivided()) {
        for (Octant &octant : m_octants) {
            AssertThrow(octant.octree != nullptr);

            octant.octree->SetParent(this);
        }
    } else {
        if (m_parent) {
            m_parent->RebuildEntry();
        }
    }
}

void LightmapOctree::InitOctants()
{
    const Vector3 divided_aabb_dimensions = m_aabb.GetExtent() / 2.0f;

    for (uint x = 0; x < 2; x++) {
        for (uint y = 0; y < 2; y++) {
            for (uint z = 0; z < 2; z++) {
                const uint index = 4 * x + 2 * y + z;

                m_octants[index] = {
                    nullptr,
                    BoundingBox(
                        m_aabb.GetMin() + divided_aabb_dimensions * Vec3f(float(x), float(y), float(z)),
                        m_aabb.GetMin() + divided_aabb_dimensions * (Vec3f(float(x), float(y), float(z)) + Vec3f(1.0f))
                    )
                };
            }
        }
    }
}

LightmapOctree *LightmapOctree::GetChildOctant(OctantID octant_id)
{
    if (octant_id == OctantID::invalid) {
        return nullptr;
    }

    if (octant_id == m_octant_id) {
        return this;
    }

    if (octant_id.depth <= m_octant_id.depth) {
        return nullptr;
    }

    LightmapOctree *current = this;

    for (uint depth = m_octant_id.depth + 1; depth <= octant_id.depth; depth++) {
        const uint8 index = octant_id.GetIndex(depth);

        if (!current || !current->IsDivided()) {
            return nullptr;
        }

        current = current->m_octants[index].octree.Get();
    }

    return current;
}

void LightmapOctree::Divide()
{
    AssertThrow(!IsDivided());

    for (uint i = 0; i < 8; ++i) {
        Octant &octant = m_octants[i];
        AssertThrow(octant.octree == nullptr);

        octant.octree.Reset(new LightmapOctree(octant.aabb, this, uint8(i)));
    }

    m_is_divided = true;
}

void LightmapOctree::Undivide()
{
    AssertThrow(IsDivided());

    for (Octant &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        if (octant.octree->IsDivided()) {
            octant.octree->Undivide();
        }

        octant.octree.Reset();
    }

    m_is_divided = false;
}

void LightmapOctree::Clear()
{
    Array<LightmapOctreeEntry> tmp;

    Clear(tmp);
}

void LightmapOctree::Clear(Array<LightmapOctreeEntry> &out_entries)
{
    ClearInternal(out_entries);

    if (IsDivided()) {
        Undivide();
    }
}

void LightmapOctree::ClearInternal(Array<LightmapOctreeEntry> &out_entries)
{
    // Only add the entry if we are at the lowest level
    if (!m_is_divided) {
        out_entries.PushBack(std::move(m_entry));

        return;
    }

    for (Octant &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);

        octant.octree->ClearInternal(out_entries);
    }
}

LightmapOctree::InsertResult LightmapOctree::Insert(LightmapOctreeEntry entry)
{
    if (!m_aabb.ContainsPoint(entry.hitpoint)) {
        auto rebuild_result = RebuildExtendInternal(entry.hitpoint);

        if (!rebuild_result.first) {
            return rebuild_result;
        }
    }

    // stop recursing if we are at max depth
    if (m_octant_id.depth < OctantID::max_depth - 1) {
        for (Octant &octant : m_octants) {
            if (octant.aabb.ContainsPoint(entry.hitpoint)) {
                if (!IsDivided()) {
                    Divide();
                }

                AssertThrow(octant.octree != nullptr);

                return octant.octree->Insert(std::move(entry));
            }
        }
    }

    return InsertInternal(std::move(entry));
}

LightmapOctree::InsertResult LightmapOctree::InsertInternal(LightmapOctreeEntry entry)
{
    m_entry = std::move(entry);

    if (m_parent) {
        m_parent->RebuildEntry();
    }

    return {
        { LightmapOctree::Result::OCTREE_OK },
        m_octant_id
    };
}

LightmapOctree::InsertResult LightmapOctree::Rebuild()
{
    Array<LightmapOctreeEntry> new_entries;
    Clear(new_entries);

    BoundingBox prev_aabb = m_aabb;

    if (IsRoot()) {
        m_aabb = BoundingBox::empty;
    }

    for (auto &entry : new_entries) {
        if (IsRoot()) {
            m_aabb.Extend(entry.hitpoint);
        } else {
            AssertThrow(m_aabb.ContainsPoint(entry.hitpoint));
        }

        auto insert_result = Insert(std::move(entry));

        if (!insert_result.first) {
            return insert_result;
        }
    }

    return {
        { LightmapOctree::Result::OCTREE_OK },
        m_octant_id
    };
}

LightmapOctree::InsertResult LightmapOctree::Rebuild(const BoundingBox &new_aabb)
{
    Array<LightmapOctreeEntry> new_entries;
    Clear(new_entries);

    m_aabb = new_aabb;

    for (auto &entry : new_entries) {
        auto insert_result = Insert(std::move(entry));

        if (!insert_result.first) {
            return insert_result;
        }
    }

    return {
        { LightmapOctree::Result::OCTREE_OK },
        m_octant_id
    };
}

LightmapOctree::InsertResult LightmapOctree::RebuildExtendInternal(Vec3f extend_include_point)
{
    // have to grow the aabb by rebuilding the octree
    BoundingBox new_aabb(m_aabb);
    // extend the new aabb to include the point
    new_aabb.Extend(extend_include_point);
    // grow our new aabb by a predetermined growth factor,
    // to keep it from constantly resizing
    new_aabb *= growth_factor;

    return Rebuild(new_aabb);
}

void LightmapOctree::RebuildEntry(uint depth)
{
    if (IsBottomLevel()) {
        // only care about parent octants
        return;
    }

    AssertThrow(IsDivided());

    LightmapOctreeEntry new_entry { };

    for (Octant &octant : m_octants) {
        AssertThrow(octant.octree != nullptr);
        
        new_entry.hitpoint += octant.octree->GetValue().hitpoint;
        new_entry.color += octant.octree->GetValue().color;
    }

    new_entry.hitpoint /= 8.0f;
    new_entry.color /= 8.0f;
    
    m_entry = std::move(new_entry);

    if (m_parent) {
        m_parent->RebuildEntry(depth + 1);
    }
}

} // namespace hyperion::v2