/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern CVar<bool> g_cvGlimmerBricksEnabled;
extern CVar<int> g_cvGlimmerBricksPoolBricks;     // read when the pool is created
extern CVar<int> g_cvGlimmerBricksPoolPages;      // pages of 256 crossings, read when the pool is created
extern CVar<int> g_cvGlimmerBricksPoolCards;      // read when the pool is created
extern CVar<int> g_cvGlimmerBricksRelightBricks;  // bricks whose cards are lit again a frame
extern CVar<int> g_cvGlimmerBricksChunksPerFrame; // chunks of 8x8x8 cells built a frame at most
extern CVar<int> g_cvGlimmerBricksProbeRays;      // 0 = the probes trace the BVH, 1 = the bricks
extern CVar<bool> g_cvGlimmerBricksReflections;   // glossy reflections from the bricks, where they are built
extern CVar<bool> g_cvGlimmerBricksReflectionsSlots;    // off has every pixel trace for itself
extern CVar<bool> g_cvGlimmerBricksReflectionsTemporal; // adds frames up
extern CVar<int> g_cvGlimmerBricksReflectionsView;      // 1 colours pixels by how they came by their reflection: green slots, blue slots still coming, red own ray, magenta filling in, black handed back
extern CVar<bool> g_cvGlimmerBricksReflectionsStats;    // counts requests and listed rays into Rendering/Glimmer/BrickReflection* stats

} // namespace Hyperion
