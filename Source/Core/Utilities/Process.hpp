/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

namespace Hyperion {

CORE_API uint32 GetCurrentProcessIdentifier();

// false only when the process is known to have exited
CORE_API bool IsProcessRunning(uint32 processIdentifier);

} // namespace Hyperion
