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
extern CVar<bool> g_cvGlimmerSHNearestHitLight;              // SH rays light their hits from the nearest voxel (cheap) rather than the filtered volume
extern CVar<int> g_cvGlimmerSHRefreshVoxelsPerFrame;         // SH voxels traced again a frame while nothing changes, in slices of half a cascade column

extern CVar<float> g_cvGlimmerVisibility;
extern CVar<bool> g_cvGlimmerLookAhead;
extern CVar<float> g_cvGlimmerStartupBurstSeconds;
extern CVar<int> g_cvGlimmerBuildThreads;
extern CVar<float> g_cvGlimmerBVHRadius;                     // how far from the viewer's cell instances go into the TLAS
extern CVar<float> g_cvGlimmerBVHCellSize;                   // the TLAS and its LODs follow the viewer a cell (xz) at a time
extern CVar<int> g_cvGlimmerBVHNodePoolMB;                   // BLAS nodes for tracing the triangle cache; 0 builds no BVH

} // namespace Hyperion
