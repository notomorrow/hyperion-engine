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
CVar<int> g_cvGlimmerBricksPoolCards("Rendering.Glimmer.Bricks.PoolCards", 16384);
CVar<int> g_cvGlimmerBricksRelightBricks("Rendering.Glimmer.Bricks.RelightBricks", 128);
CVar<int> g_cvGlimmerBricksChunksPerFrame("Rendering.Glimmer.Bricks.ChunksPerFrame", 2);
CVar<int> g_cvGlimmerBricksProbeRays("Rendering.Glimmer.Bricks.ProbeRays", 0);
CVar<bool> g_cvGlimmerBricksReflections("Rendering.Glimmer.Bricks.Reflections", true);
CVar<bool> g_cvGlimmerBricksReflectionsSlots("Rendering.Glimmer.Bricks.Reflections.Slots", true);
CVar<bool> g_cvGlimmerBricksReflectionsTemporal("Rendering.Glimmer.Bricks.Reflections.Temporal", true);
CVar<int> g_cvGlimmerBricksReflectionsView("Rendering.Glimmer.Bricks.Reflections.View", 0);
CVar<bool> g_cvGlimmerBricksReflectionsStats("Rendering.Glimmer.Bricks.Reflections.Stats", false);

} // namespace Hyperion
