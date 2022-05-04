#ifndef HYPERION_V2_VISIBILITY_STATE_H
#define HYPERION_V2_VISIBILITY_STATE_H

#include "scene.h"

#include <util.h>

#include <cstdint>

namespace hyperion::v2 {

struct VisibilityState {
    using Bitmask = uint64_t;
    using Nonce   = uint16_t;

    static constexpr uint32_t max_scenes = sizeof(Bitmask) * CHAR_BIT;

    /* map from scene index (id - 1) -> visibility boolean for each frame in flight.
     * when visiblity is scanned, both values per frame in flight are set accordingly,
     * but are only set to false after the corresponding frame is rendered.
     */
    
    Bitmask bits  = 0;
    Nonce nonce   = 0;

    HYP_FORCE_INLINE bool Get(Scene::ID scene) const
    {
        AssertThrow(scene.value - 1ull < max_scenes);
        
        return bits & 1ull << static_cast<Bitmask>(scene.value - 1);
    }

    HYP_FORCE_INLINE void Set(Scene::ID scene, bool visible)
    {
        AssertThrow(scene.value - 1 < max_scenes);

        if (visible) {
            bits |= 1ull << static_cast<Bitmask>(scene.value - 1);
        } else {
            bits &= ~(1ull << (scene.value - 1));
        }
    }

    HYP_FORCE_INLINE bool ValidToParent(const VisibilityState &parent) const
        { return nonce == parent.nonce; }
};

} // namespace hyperion::v2

#endif