/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/functional/Delegate.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/Renderer.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/RenderObject.hpp>

#include <scene/Scene.hpp>
#include <scene/Subsystem.hpp>

namespace hyperion {

class UIStage;
class UIObject;
class RenderCamera;
class View;
class RenderView;
struct RenderSetup;
struct UIPassData;

class UIRenderCollector : public RenderCollector
{
public:
    UIRenderCollector() = default;
    ~UIRenderCollector() = default;

    void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer, uint32 bucketBits);
};

class UIRenderer : public RendererBase
{
public:
    UIRenderer(const Handle<View>& view);
    virtual ~UIRenderer() = default;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& renderSetup) override;

    UIRenderCollector renderCollector;

protected:
    PassData* CreateViewPassData(View* view, PassDataExt&) override;

    Handle<View> m_view;
};

HYP_CLASS(NoScriptBindings)
class HYP_API UIRenderSubsystem : public Subsystem
{
    HYP_OBJECT_BODY(UIRenderSubsystem);

public:
    UIRenderSubsystem(const Handle<UIStage>& uiStage);
    UIRenderSubsystem(const UIRenderSubsystem& other) = delete;
    UIRenderSubsystem& operator=(const UIRenderSubsystem& other) = delete;
    virtual ~UIRenderSubsystem();

    HYP_FORCE_INLINE const Handle<UIStage>& GetUIStage() const
    {
        return m_uiStage;
    }

    virtual void PreUpdate(float delta) override;
    virtual void Update(float delta) override;

private:
    virtual void Init() override;

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;

    void CreateFramebuffer();

    Handle<UIStage> m_uiStage;

    ShaderRef m_shader;

    TResourceHandle<RenderCamera> m_cameraResourceHandle;

    Handle<View> m_view;

    UIRenderer* m_uiRenderer;

    DelegateHandler m_onGbufferResolutionChangedHandle;
};

} // namespace hyperion

