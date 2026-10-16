/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerCVars.hpp>

namespace Hyperion {

CVar<bool> g_cvGlimmerEnabled("Rendering.Glimmer.Enabled", true);

CVar<float> g_cvGlimmerSpansRadius("Rendering.Glimmer.Spans.Radius", 384.0f);
CVar<float> g_cvGlimmerFoliageExtinction("Rendering.Glimmer.Foliage.Extinction", 1.0f);

CVar<float> g_cvGlimmerIntensity("Rendering.Glimmer.Intensity", 1.0f);
CVar<int> g_cvGlimmerDebugView("Rendering.Glimmer.DebugView", 0);
CVar<bool> g_cvGlimmerHalfRes("Rendering.Glimmer.HalfRes", true);
CVar<int> g_cvGlimmerDebugSH("Rendering.Glimmer.DebugSH", 0);
CVar<float> g_cvGlimmerVisibility("Rendering.Glimmer.Visibility", 1.0f);
CVar<bool> g_cvGlimmerLookAhead("Rendering.Glimmer.LookAhead", true);

} // namespace Hyperion
