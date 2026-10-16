/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Bricks/GlimmerBrickCVars.hpp>

namespace Hyperion {

CVar<bool> g_cvGlimmerBricksEnabled("Rendering.Glimmer.Bricks.Enabled", true);
CVar<int> g_cvGlimmerBricksPoolBudgetMB("Rendering.Glimmer.Bricks.PoolBudgetMB", 256);
CVar<int> g_cvGlimmerBricksInitialPoolMB("Rendering.Glimmer.Bricks.InitialPoolMB", 0);
CVar<int> g_cvGlimmerBricksChunksPerFrame("Rendering.Glimmer.Bricks.ChunksPerFrame", 4);
CVar<bool> g_cvGlimmerBricksTextured("Rendering.Glimmer.Bricks.Textured", true);
CVar<bool> g_cvGlimmerBricksReflections("Rendering.Glimmer.Bricks.Reflections", true);
} // namespace Hyperion
