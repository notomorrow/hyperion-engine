#pragma once

#include <Game/NativeTestTypes.hpp>

namespace Hyperion {

// derives from another game class and lives in its own translation unit, so it registers in a different one from its parent
HYP_CLASS()
class NativeTestMedkit final : public NativeTestPickup
{
    HYP_OBJECT_BODY(NativeTestMedkit);

public:
    HYP_FIELD(Property = "HealAmount", Serialize)
    float healAmount = 0.0f;

    HYP_METHOD()
    float GetTotalHealing() const;
};

} // namespace Hyperion
