#include <Game/NativeTestMedkit.hpp>

#include <NativeTestMedkit.generated.inl>

namespace Hyperion {

float NativeTestMedkit::GetTotalHealing() const
{
    return healAmount * float(quantity);
}

} // namespace Hyperion
