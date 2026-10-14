/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Framework/CVarManager.hpp>

namespace Hyperion {

extern CVar<bool> g_cvGlimmerBricksEnabled;
extern CVar<int> g_cvGlimmerBricksPoolBudgetMB;      // a pool that runs short grows in place, doubling or by what still fits; past it, far chunks give way to near ones
extern CVar<int> g_cvGlimmerBricksChunksPerFrame;    // chunks of 8x8x8 cells built a frame at most
extern CVar<bool> g_cvGlimmerBricksTextured;         // crossings take their colour and finish from the textures; off takes each material's average
extern CVar<bool> g_cvGlimmerBricksReflections;      // glossy reflections from the bricks, where they are built
extern CVar<int> g_cvGlimmerBricksReflectionsView;   // 1 colours pixels by how they came by their reflection: green slots, blue slots still coming, red own ray, magenta filling in, black handed back
extern CVar<bool> g_cvGlimmerBricksReflectionsStats; // counts requests and listed rays into Rendering/Glimmer/BrickReflection* stats, and what the glossy share of the picture came from into BrickReflectionFrom/* (per mille): bricks, span solids past the bricks, ground, canopy, sky

} // namespace Hyperion
