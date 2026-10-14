/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Bricks/GlimmerBrickCVars.hpp>

namespace Hyperion {

CVar<bool> g_cvGlimmerBricksEnabled("Rendering.Glimmer.Bricks.Enabled", false);
CVar<int> g_cvGlimmerBricksPoolBricks("Rendering.Glimmer.Bricks.PoolBricks", 4096);
CVar<int> g_cvGlimmerBricksPoolPages("Rendering.Glimmer.Bricks.PoolPages", 16384);
CVar<int> g_cvGlimmerBricksChunksPerFrame("Rendering.Glimmer.Bricks.ChunksPerFrame", 2);

} // namespace Hyperion
