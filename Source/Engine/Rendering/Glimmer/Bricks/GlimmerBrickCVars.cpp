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
CVar<float> g_cvGlimmerBricksCellSize("Rendering.Glimmer.Bricks.CellSize", 2.0f);
CVar<int> g_cvGlimmerBricksLevels("Rendering.Glimmer.Bricks.Levels", 4);
CVar<int> g_cvGlimmerBricksLevelScale("Rendering.Glimmer.Bricks.LevelScale", 4);
} // namespace Hyperion
