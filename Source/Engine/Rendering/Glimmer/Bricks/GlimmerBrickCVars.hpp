/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

    
extern CVar<bool> g_cvGlimmerBricksEnabled;
extern CVar<int> g_cvGlimmerBricksPoolBudgetMB;         // a pool that runs short grows in place, doubling or by what still fits.
extern CVar<int> g_cvGlimmerBricksInitialPoolMB;        // the pools start at this size, within the budget
extern CVar<int> g_cvGlimmerBricksChunksPerFrame;       // chunks of 8x8x8 cells built a frame at most
extern CVar<bool> g_cvGlimmerBricksTextured;            // crossings take their color from textures. otherwise will use avg from material
extern CVar<bool> g_cvGlimmerBricksReflections;         // glossy reflections from the bricks
} // namespace Hyperion
