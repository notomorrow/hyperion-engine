/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern CVar<bool> g_cvGlimmerBricksEnabled;
extern CVar<int> g_cvGlimmerBricksPoolBricks;     // read when the pool is created
extern CVar<int> g_cvGlimmerBricksPoolPages;      // pages of 256 crossings, read when the pool is created
extern CVar<int> g_cvGlimmerBricksChunksPerFrame; // chunks of 8x8x8 cells built a frame at most

} // namespace Hyperion
