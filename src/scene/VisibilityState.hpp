#ifndef HYPERION_V2_VISIBILITY_STATE_H
#define HYPERION_V2_VISIBILITY_STATE_H

#include <core/Containers.hpp>
#include <core/ID.hpp>
#include <math/MathUtil.hpp>
#include <Types.hpp>

#include <atomic>
#include <cstdint>

namespace hyperion::v2 {

class Camera;

struct VisibilityStateSnapshot
{
    using Bitmask   = uint64;
    using Nonce     = uint16;

    Bitmask bits { 0u };
    Nonce   nonce { 0u };

    VisibilityStateSnapshot()                                                       = default;
    VisibilityStateSnapshot(const VisibilityStateSnapshot &other)                   = default;
    VisibilityStateSnapshot &operator=(const VisibilityStateSnapshot &other)        = default;
    VisibilityStateSnapshot(VisibilityStateSnapshot &&other) noexcept               = default;
    VisibilityStateSnapshot &operator=(VisibilityStateSnapshot &&other) noexcept    = default;
    ~VisibilityStateSnapshot()                                                      = default;

    HYP_FORCE_INLINE
    Bitmask Get(ID<Camera> id) const
        { return bits & (1ull << Bitmask(id.ToIndex())); }

    HYP_FORCE_INLINE
    void Set(ID<Camera> id, bool visible)
    {
        if (visible) {
            bits |= (1ull << Bitmask(id.ToIndex()));
        } else {
            bits &= (~(1ull << id.ToIndex()));
        }
    }

    HYP_FORCE_INLINE
    bool ValidToParent(const VisibilityStateSnapshot &parent) const
        { return nonce == parent.nonce; }
};

struct VisibilityState
{
    using Bitmask   = VisibilityStateSnapshot::Bitmask;
    using Nonce     = VisibilityStateSnapshot::Nonce;

    static constexpr uint max_visibility_states = sizeof(Bitmask) * CHAR_BIT;
    static constexpr uint cursor_size = 3u;

    FixedArray<VisibilityStateSnapshot, cursor_size> snapshots { };

    VisibilityState()                                               = default;
    VisibilityState(const VisibilityState &other)                   = default;
    VisibilityState &operator=(const VisibilityState &other)        = default;
    VisibilityState(VisibilityState &&other) noexcept               = default;
    VisibilityState &operator=(VisibilityState &&other) noexcept    = default;
    ~VisibilityState()                                              = default;

    HYP_FORCE_INLINE
    bool Get(ID<Camera> id, uint8 cursor) const
        { return snapshots[cursor].Get(id); }

    HYP_FORCE_INLINE
    void SetVisible(ID<Camera> id, uint8 cursor)
        { snapshots[cursor].Set(id, true); }

    HYP_FORCE_INLINE
    void SetHidden(ID<Camera> id, uint8 cursor)
        { snapshots[cursor].Set(id, false); }

    HYP_FORCE_INLINE
    bool ValidToParent(const VisibilityState &parent, uint8 cursor) const
        { return snapshots[cursor].ValidToParent(parent.snapshots[cursor]); }

    void ForceAllVisible()
    {
        for (auto &snapshot : snapshots) {
            snapshot.bits = MathUtil::MaxSafeValue<Bitmask>();
            snapshot.nonce = MathUtil::MaxSafeValue<Nonce>();
        }
    }
};

} // namespace hyperion::v2

#endif