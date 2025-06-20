/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCREEN_CAPTURE_HPP
#define HYPERION_SCREEN_CAPTURE_HPP

#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderResource.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/BoundingBox.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <core/containers/FixedArray.hpp>

#include <core/object/HypObject.hpp>

#include <Types.hpp>

namespace hyperion {

class RenderView;

enum class ScreenCaptureMode
{
    TO_TEXTURE,
    TO_BUFFER
};

HYP_CLASS()
class HYP_API ScreenCaptureRenderSubsystem : public RenderSubsystem
{
    HYP_OBJECT_BODY(ScreenCaptureRenderSubsystem);

public:
    ScreenCaptureRenderSubsystem(Name name, const TResourceHandle<RenderView>& view, ScreenCaptureMode screen_capture_mode = ScreenCaptureMode::TO_TEXTURE);
    ScreenCaptureRenderSubsystem(const ScreenCaptureRenderSubsystem& other) = delete;
    ScreenCaptureRenderSubsystem& operator=(const ScreenCaptureRenderSubsystem& other) = delete;
    ScreenCaptureRenderSubsystem(ScreenCaptureRenderSubsystem&& other) noexcept = delete;
    ScreenCaptureRenderSubsystem& operator=(ScreenCaptureRenderSubsystem&& other) noexcept = delete;
    virtual ~ScreenCaptureRenderSubsystem() override;

    HYP_FORCE_INLINE const GPUBufferRef& GetBuffer() const
    {
        return m_buffer;
    }

    HYP_FORCE_INLINE const Handle<Texture>& GetTexture() const
    {
        return m_texture;
    }

private:
    virtual void Init() override;
    virtual void InitGame() override;
    virtual void OnRemoved() override;
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(FrameBase* frame, const RenderSetup& render_setup) override;

    TResourceHandle<RenderView> m_view;
    ScreenCaptureMode m_screen_capture_mode;
    Handle<Texture> m_texture;
    GPUBufferRef m_buffer;
};

} // namespace hyperion

#endif