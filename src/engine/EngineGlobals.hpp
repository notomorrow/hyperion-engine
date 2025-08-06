/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/object/Handle.hpp>

#include <core/Types.hpp>

namespace hyperion {

class EngineDriver;
class AssetManager;
class SafeDeleter;
class ShaderManager;
class MaterialCache;
class RenderGlobalState;
class IRenderBackend;
class ShaderCompiler;
class EditorState;

#ifdef HYP_BUILD_LIBRARY

// Globals for internal usage within the Hyperion library

extern Handle<EngineDriver> g_engineDriver;
extern Handle<AssetManager> g_assetManager;
extern Handle<EditorState> g_editorState;
extern ShaderManager* g_shaderManager;
extern MaterialCache* g_materialSystem;
extern SafeDeleter* g_safeDeleter;
extern IRenderBackend* g_renderBackend;
extern RenderGlobalState* g_renderGlobalState;
extern ShaderCompiler* g_shaderCompiler;

#endif

} // namespace hyperion
