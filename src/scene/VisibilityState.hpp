/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/ObjId.hpp>

#include <Types.hpp>

namespace hyperion {

class Camera;

struct VisibilityStateSnapshot
{
    uint16 validityMarker { 0u };

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityStateSnapshot& parent) const
    {
        return validityMarker == parent.validityMarker;
    }
};

struct VisibilityState
{
    uint16 validityMarker { 0u };
    HashMap<ObjId<Camera>, VisibilityStateSnapshot> snapshots;

    VisibilityState() = default;
    VisibilityState(const VisibilityState& other) = default;
    VisibilityState& operator=(const VisibilityState& other) = default;
    VisibilityState(VisibilityState&& other) noexcept = default;
    VisibilityState& operator=(VisibilityState&& other) noexcept = default;
    ~VisibilityState() = default;

    HYP_FORCE_INLINE void Next()
    {
        ++validityMarker;
    }

    HYP_FORCE_INLINE VisibilityStateSnapshot GetSnapshot(ObjId<Camera> id) const
    {
        auto it = snapshots.Find(id);

        if (it == snapshots.End())
        {
            return {};
        }

        return it->second;
    }

    HYP_FORCE_INLINE void MarkAsValid(ObjId<Camera> id)
    {
        auto it = snapshots.Find(id);

        if (it == snapshots.End())
        {
            snapshots.Insert(id, VisibilityStateSnapshot { validityMarker });
        }
        else
        {
            it->second.validityMarker = validityMarker;
        }
    }
};

} // namespace hyperion
