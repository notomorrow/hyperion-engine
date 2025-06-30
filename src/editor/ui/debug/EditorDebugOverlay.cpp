/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <editor/ui/debug/EditorDebugOverlay.hpp>

#include <core/threading/Threads.hpp>

#include <core/logging/Logger.hpp>

#include <scene/Texture.hpp>

#include <ui/UIImage.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Editor);

#pragma region EditorDebugOverlayBase

EditorDebugOverlayBase::EditorDebugOverlayBase()
{
}

EditorDebugOverlayBase::~EditorDebugOverlayBase()
{
}

void EditorDebugOverlayBase::Initialize(UIObject* spawnParent)
{
    Threads::AssertOnThread(g_gameThread);
    AssertThrow(spawnParent != nullptr);

    m_uiObject = CreateUIObject(spawnParent);
}

Handle<UIObject> EditorDebugOverlayBase::CreateUIObject_Impl(UIObject* spawnParent)
{
    return spawnParent->CreateUIObject<UIImage>(GetName(), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PIXEL }, { 75, UIObjectSize::PIXEL }));
}

#pragma endregion EditorDebugOverlayBase

#pragma region TextureEditorDebugOverlay

TextureEditorDebugOverlay::TextureEditorDebugOverlay(const Handle<Texture>& texture)
    : m_texture(texture)
{
}

TextureEditorDebugOverlay::~TextureEditorDebugOverlay()
{
}

Handle<UIObject> TextureEditorDebugOverlay::CreateUIObject_Impl(UIObject* spawnParent)
{
    InitObject(m_texture);

    Handle<UIImage> image = spawnParent->CreateUIObject<UIImage>(GetName(), Vec2i::Zero(), UIObjectSize({ 100, UIObjectSize::PIXEL }, { 75, UIObjectSize::PIXEL }));
    image->SetTexture(m_texture);

    return image;
}

#pragma endregion TextureEditorDebugOverlay

} // namespace hyperion