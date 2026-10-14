/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern CVar<int> g_cvGlimmerProbesNearFieldCascades;

// the probe blocks; off leaves the near field to the SH voxels
extern CVar<bool> g_cvGlimmerProbesEnabled;
extern CVar<int> g_cvGlimmerProbesRaysPerFrame;            // probes traced a frame = this / rays per probe
extern CVar<int> g_cvGlimmerProbesPoolBlocks;              // blocks of 64 probes the pool holds at most (up to GlimmerProbePoolBlocks)
extern CVar<float> g_cvGlimmerProbesBlockMargin;           // how far (in probe spacings) around a block solids make it wanted
extern CVar<float> g_cvGlimmerProbesMinHeightAboveGround;  // solids lower than this on the ground (roads, rocks) want no probes
extern CVar<int> g_cvGlimmerProbesBlockReleaseFrames;      // frames an unwanted block keeps its probes
extern CVar<float> g_cvGlimmerProbesRelocateMargin;        // how far past a back face a probe inside a solid moves (m)
extern CVar<int> g_cvGlimmerProbesInsideRetryInterval;     // updates between checks on probes stuck inside solids
extern CVar<int> g_cvGlimmerProbesClassifyPeriod;          // frames between looks at the solids around a block
extern CVar<bool> g_cvGlimmerProbesLogStats;               // logs how many probes there are (per level, by state, and on screen) once a second

extern CVar<float> g_cvGlimmerProbesHistorySeconds;
extern CVar<float> g_cvGlimmerProbesHistorySecondsChanging;
extern CVar<int> g_cvGlimmerProbesMinHistory;
extern CVar<int> g_cvGlimmerProbesMinHistoryChanging;

extern CVar<bool> g_cvGlimmerProbesSleep;
extern CVar<int> g_cvGlimmerProbesSleepMaxInterval;
extern CVar<int> g_cvGlimmerProbesSafetyRefreshSeconds;

enum class GlimmerProbeDebugMode : int
{
    None = 0,

    Status,
    Irradiance,
    IrradianceTowardViewer,

    Max
};

extern CVar<int> g_cvGlimmerProbesDebug;
extern CVar<int> g_cvGlimmerProbesDebugLevel;      // which level's probes to draw, -1 for all
extern CVar<float> g_cvGlimmerProbesDebugRadius;   // only probes this close to the viewer
extern CVar<float> g_cvGlimmerProbesDebugExposure; // scales irradiance before it's tonemapped for display


} // namespace Hyperion
