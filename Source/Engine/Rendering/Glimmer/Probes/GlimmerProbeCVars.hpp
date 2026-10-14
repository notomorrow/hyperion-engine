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
extern CVar<int> g_cvGlimmerProbesRaysPerFrame; // probes traced a frame = this / rays per probe
extern CVar<int> g_cvGlimmerProbesPoolBlocks;   // blocks of 64 probes the pool holds at most (up to GlimmerProbePoolBlocks)
extern CVar<bool> g_cvGlimmerProbesLogStats;    // logs how many probes there are (per level, by state, and on screen) once a second
extern CVar<bool> g_cvGlimmerProbesSleep;

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
