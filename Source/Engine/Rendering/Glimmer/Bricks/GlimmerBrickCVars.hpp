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
extern CVar<float> g_cvGlimmerBricksCellSize;           // the finest level's cells, in metres; 32 columns a cell at every level
extern CVar<int> g_cvGlimmerBricksLevels;
extern CVar<int> g_cvGlimmerBricksLevelScale;            // how many times a level's cells are the size of the finer level's: 2 or 4               // levels of cells, each twice the size of the one before, 1 to GlimmerBrickLevels
} // namespace Hyperion
