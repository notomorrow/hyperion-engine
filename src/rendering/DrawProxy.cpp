#include <rendering/DrawProxy.hpp>
#include <Engine.hpp>

namespace hyperion::v2 {

void WaitForRenderUpdatesToComplete()
{
    HYP_SYNC_RENDER();
}

} // namespace hyperion::v2