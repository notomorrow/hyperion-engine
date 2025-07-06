/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Handle.hpp>

#include <Types.hpp>

namespace hyperion {

class Engine;
class AssetManager;
class SafeDeleter;
class ShaderManager;
class MaterialCache;
class RenderGlobalState;
class IRenderBackend;
class ShaderCompiler;

#ifdef HYP_BUILD_LIBRARY

// Globals for internal usage within the Hyperion library

extern Handle<Engine> g_engine;
extern Handle<AssetManager> g_assetManager;
extern ShaderManager* g_shaderManager;
extern MaterialCache* g_materialSystem;
extern SafeDeleter* g_safeDeleter;
extern IRenderBackend* g_renderBackend;
extern RenderGlobalState* g_renderGlobalState;
extern ShaderCompiler* g_shaderCompiler;

#endif

} // namespace hyperion
