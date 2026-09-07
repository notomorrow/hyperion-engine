/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/BakeLayer.hpp>

#include <BakeLayer.generated.inl>

namespace Hyperion {
namespace Baking {

#pragma region BakeLayerHashes

BakeLayerHashes::BakeLayerHashes()
{
    Memory::Zero(this, sizeof(BakeLayerHashes));
}

#pragma endregion BakeLayerHashes

} // namespace Baking
} // namespace Hyperion
