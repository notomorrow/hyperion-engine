/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

enum class GlimmerDebugView : int
{
    None = 0,

    Irradiance,
    Coverage,
    TechniqueFirst
};

enum class GlimmerHeightfieldDebugView : int
{
    None = 0,
    Spans,
    GroundAlbedo,
    Occupancy,

    Max
};

extern CVar<bool> g_cvGlimmerEnabled;

// how far around the viewer static solids and foliage are splatted into the heightfield
extern CVar<float> g_cvGlimmerSpansRadius;

extern CVar<float> g_cvGlimmerFoliageExtinction;

extern CVar<float> g_cvGlimmerIntensity;
extern CVar<int> g_cvGlimmerDebugView;

extern CVar<bool> g_cvGlimmerHalfRes; // irradiance evaluated for one pixel of each 2x2 quad, then upsampled

// 0 = off
// 1 = irradiance
// 2 = sky visibility
// 3 = sky visibility of the single voxel (no interpolation)
extern CVar<int> g_cvGlimmerDebugSH;

extern CVar<float> g_cvGlimmerVisibility;
extern CVar<bool> g_cvGlimmerLookAhead;
extern CVar<float> g_cvGlimmerStartupBurstSeconds;
extern CVar<int> g_cvGlimmerBuildThreads;

// diffuse GI from the bricks' slots and cards in place of the SH grid
extern CVar<bool> g_cvGlimmerBrickGI;

// live bricks whose GI slots are traced again each frame, so ones that hit a place since given to another brick come right
extern CVar<int> g_cvGlimmerBrickGIRetraceBricks;

} // namespace Hyperion
