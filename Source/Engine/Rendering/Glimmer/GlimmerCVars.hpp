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

// least horizontal half extent of the scene region, which recenters on the viewer in steps of a quarter of it
extern CVar<float> g_cvGlimmerRegionMinRadius;

// how far around the viewer static solids and foliage are splatted into the heightfield
extern CVar<float> g_cvGlimmerSpansRadius;

extern CVar<float> g_cvGlimmerFoliageExtinction;
// leaves bunch up in crowns with gaps between them, so a texel's leaf area blocks less than if it were spread evenly
extern CVar<float> g_cvGlimmerFoliageClumping;

extern CVar<float> g_cvGlimmerIntensity;
extern CVar<int> g_cvGlimmerDebugView;

extern CVar<bool> g_cvGlimmerHalfRes;            // irradiance evaluated for one pixel of each 2x2 quad, then upsampled
extern CVar<bool> g_cvGlimmerSpecularOcclusion;  // sky reflections take Glimmer's sky visibility along the reflection

// 0 = off
// 1 = irradiance
// 2 = sky visibility
// 3 = sky visibility of the single voxel (no interpolation)
extern CVar<int> g_cvGlimmerDebugSH;

// how much the probes' and SH voxels' depth moments cut off light from behind walls: 1 = fully, 0 = not at all (for comparing)
extern CVar<float> g_cvGlimmerVisibility;

extern CVar<float> g_cvGlimmerGroundAlbedo; // where the terrain's own albedo isn't known

extern CVar<bool> g_cvGlimmerRunOnChange;
extern CVar<bool> g_cvGlimmerSkipLodOnlyChanges;   // an instance that only changed LOD does not retrace the SH voxels or wake the probes around it
extern CVar<float> g_cvGlimmerSunSHWindow;         // how much the sun's higher SH bands are damped. 0 = sharpest and rings, 1 = widest
extern CVar<int> g_cvGlimmerSHRefreshVoxels;       // per frame while the lighting holds
extern CVar<int> g_cvGlimmerRelightTexels;

/// @TODO Move to GlimmerHelpers?
bool IsGlimmerSceneRequired();

} // namespace Hyperion
