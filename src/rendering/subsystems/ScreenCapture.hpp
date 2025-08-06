/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <scene/Subsystem.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <rendering/RenderObject.hpp>

#include <core/object/Handle.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/functional/Delegate.hpp>

#include <core/object/HypObject.hpp>

#include <core/Types.hpp>

namespace hyperion {

class View;
class Texture;

enum class ScreenCaptureMode
{
    TO_TEXTURE,
    TO_BUFFER
};

HYP_CLASS(NoScriptBindings)
class HYP_API ScreenCaptureRenderSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(ScreenCaptureRenderSubsystem);

public:
    ScreenCaptureRenderSubsystem(const Handle<View>& view, ScreenCaptureMode screenCaptureMode = ScreenCaptureMode::TO_TEXTURE);
    ScreenCaptureRenderSubsystem(const ScreenCaptureRenderSubsystem& other) = delete;
    ScreenCaptureRenderSubsystem& operator=(const ScreenCaptureRenderSubsystem& other) = delete;
    ScreenCaptureRenderSubsystem(ScreenCaptureRenderSubsystem&& other) noexcept = delete;
    ScreenCaptureRenderSubsystem& operator=(ScreenCaptureRenderSubsystem&& other) noexcept = delete;
    virtual ~ScreenCaptureRenderSubsystem() override;

    HYP_FORCE_INLINE const GpuBufferRef& GetBuffer() const
    {
        return m_buffer;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetTexture() const
    {
        return m_texture;
    }

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;
    virtual void Update(float delta) override;

    Delegate<void, Handle<Texture>> OnTextureResize;

private:
    virtual void Init() override;

    void CaptureFrame(FrameBase* frame);

    Handle<View> m_view;
    ScreenCaptureMode m_screenCaptureMode;
    Handle<Texture> m_texture;
    GpuBufferRef m_buffer;
};

} // namespace hyperion

