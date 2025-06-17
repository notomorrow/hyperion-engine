/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP
#define HYPERION_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP

#include <scene/Scene.hpp>
#include <scene/Light.hpp>
#include <scene/Subsystem.hpp>

#include <scene/camera/Camera.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/threading/Semaphore.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderResource.hpp>
#include <rendering/RenderShadowMap.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/BoundingBox.hpp>

#include <Types.hpp>

namespace hyperion {

using RerenderShadowsSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE, threading::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>>;

class RenderCamera;
class RenderScene;
class RenderWorld;
class RenderLight;
class View;

struct ShadowMapCameraData
{
    Matrix4 view;
    Matrix4 projection;
    BoundingBox aabb;
};

class ShadowPass final : public FullScreenPass
{
public:
    ShadowPass(
        const Handle<Scene>& parentScene,
        const TResourceHandle<RenderWorld>& worldResourceHandle,
        const TResourceHandle<RenderCamera>& cameraResourceHandle,
        const TResourceHandle<RenderShadowMap>& shadowMapResourceHandle,
        const TResourceHandle<RenderView>& viewStaticsResourceHandle,
        const TResourceHandle<RenderView>& viewDynamicsResourceHandle,
        const ShaderRef& shader,
        RerenderShadowsSemaphore* rerenderSemaphore);
    ShadowPass(const ShadowPass& other) = delete;
    ShadowPass& operator=(const ShadowPass& other) = delete;
    virtual ~ShadowPass() override;

    HYP_FORCE_INLINE const Vec3f& GetOrigin() const
    {
        return m_origin;
    }

    HYP_FORCE_INLINE void SetOrigin(const Vec3f& origin)
    {
        m_origin = origin;
    }

    virtual void CreateFramebuffer() override;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup) override;

    virtual void RenderToFramebuffer(FrameBase* frame, const RenderSetup& renderSetup, const FramebufferRef& framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

private:
    void CreateShadowMap();
    void CreateCombineShadowMapsPass();
    void CreateComputePipelines();

    Handle<Scene> m_parentScene;
    TResourceHandle<RenderWorld> m_worldResourceHandle;
    TResourceHandle<RenderCamera> m_cameraResourceHandle;
    TResourceHandle<RenderShadowMap> m_shadowMapResourceHandle;
    TResourceHandle<RenderView> m_renderViewStatics;
    TResourceHandle<RenderView> m_renderViewDynamics;

    Vec3f m_origin;
    RerenderShadowsSemaphore* m_rerenderSemaphore;

    Handle<Texture> m_shadowMapStatics;
    Handle<Texture> m_shadowMapDynamics;

    UniquePtr<FullScreenPass> m_combineShadowMapsPass;
    ComputePipelineRef m_blurShadowMapPipeline;
};

HYP_CLASS(NoScriptBindings)
class DirectionalLightShadowRenderer : public Subsystem
{
    HYP_OBJECT_BODY(DirectionalLightShadowRenderer);

public:
    DirectionalLightShadowRenderer(const Handle<Scene>& parentScene, const Handle<Light>& light, Vec2u resolution, ShadowMapFilter filterMode);
    DirectionalLightShadowRenderer(const DirectionalLightShadowRenderer& other) = delete;
    DirectionalLightShadowRenderer& operator=(const DirectionalLightShadowRenderer& other) = delete;
    virtual ~DirectionalLightShadowRenderer() override;

    HYP_FORCE_INLINE ShadowPass* GetPass() const
    {
        return m_shadowPass.Get();
    }

    HYP_FORCE_INLINE const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    HYP_FORCE_INLINE void SetAABB(const BoundingBox& aabb)
    {
        m_aabb = aabb;
    }

    HYP_FORCE_INLINE const TResourceHandle<RenderShadowMap>& GetShadowMap() const
    {
        return m_shadowMapResourceHandle;
    }

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;
    virtual void Update(float delta) override;

private:
    virtual void Init() override;     // init on render thread

    void CreateShader();

    Handle<Scene> m_parentScene;
    Handle<Light> m_light;
    Handle<View> m_viewStatics;
    Handle<View> m_viewDynamics;

    UniquePtr<ShadowPass> m_shadowPass;
    Vec2u m_resolution;
    ShadowMapFilter m_filterMode;

    RerenderShadowsSemaphore m_rerenderSemaphore;

    ShaderRef m_shader;
    Handle<Camera> m_camera;
    BoundingBox m_aabb;

    TResourceHandle<RenderShadowMap> m_shadowMapResourceHandle;

    HashCode m_cachedOctantHashCodeStatics;
    Matrix4 m_cachedViewMatrix;
};

} // namespace hyperion

#endif
