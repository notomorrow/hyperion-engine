/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Probes/GlimmerProbeCVars.hpp>

namespace Hyperion {

CVar<int> g_cvGlimmerProbesNearFieldCascades("Rendering.Glimmer.Probes.NearField.Cascades", 4);

CVar<bool> g_cvGlimmerProbesEnabled("Rendering.Glimmer.Probes.Enabled", true);
CVar<int> g_cvGlimmerProbesRaysPerFrame("Rendering.Glimmer.Probes.RaysPerFrame", 65536);
CVar<int> g_cvGlimmerProbesPoolBlocks("Rendering.Glimmer.Probes.PoolBlocks", 512);
CVar<float> g_cvGlimmerProbesBlockMargin("Rendering.Glimmer.Probes.BlockMargin", 1.0f);
CVar<float> g_cvGlimmerProbesMinHeightAboveGround("Rendering.Glimmer.Probes.MinHeightAboveGround", 0.5f);
CVar<int> g_cvGlimmerProbesBlockReleaseFrames("Rendering.Glimmer.Probes.BlockReleaseFrames", 60);
CVar<float> g_cvGlimmerProbesHistorySeconds("Rendering.Glimmer.Probes.HistorySeconds", 1.0f);
CVar<float> g_cvGlimmerProbesHistorySecondsChanging("Rendering.Glimmer.Probes.HistorySecondsChanging", 0.25f);
CVar<int> g_cvGlimmerProbesMinHistory("Rendering.Glimmer.Probes.MinHistory", 16);
CVar<int> g_cvGlimmerProbesMinHistoryChanging("Rendering.Glimmer.Probes.MinHistoryChanging", 4);
CVar<float> g_cvGlimmerProbesRelocateMargin("Rendering.Glimmer.Probes.RelocateMargin", 0.3f);
CVar<int> g_cvGlimmerProbesInsideRetryInterval("Rendering.Glimmer.Probes.InsideRetryInterval", 64);
CVar<int> g_cvGlimmerProbesClassifyPeriod("Rendering.Glimmer.Probes.ClassifyPeriod", 8);
CVar<bool> g_cvGlimmerProbesLogStats("Rendering.Glimmer.Probes.LogStats", false);
CVar<bool> g_cvGlimmerProbesSleep("Rendering.Glimmer.Probes.Sleep", true);
CVar<int> g_cvGlimmerProbesSleepMaxInterval("Rendering.Glimmer.Probes.SleepMaxInterval", 3);
CVar<int> g_cvGlimmerProbesSafetyRefreshSeconds("Rendering.Glimmer.Probes.SafetyRefreshSeconds", 0);

CVar<int> g_cvGlimmerProbesDebug("Rendering.Glimmer.Probes.Debug", 0);
CVar<int> g_cvGlimmerProbesDebugLevel("Rendering.Glimmer.Probes.Debug.Level", -1);
CVar<float> g_cvGlimmerProbesDebugRadius("Rendering.Glimmer.Probes.Debug.Radius", 32.0f);
CVar<float> g_cvGlimmerProbesDebugExposure("Rendering.Glimmer.Probes.Debug.Exposure", 1.0f);


} // namespace Hyperion
