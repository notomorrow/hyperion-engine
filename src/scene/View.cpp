/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <scene/View.hpp>
#include <scene/Scene.hpp>
#include <scene/Light.hpp>
#include <scene/Mesh.hpp>
#include <scene/Material.hpp>
#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/Texture.hpp>
#include <scene/lightmapper/LightmapVolume.hpp>
#include <scene/camera/Camera.hpp>
#include <scene/animation/Skeleton.hpp>

#include <scene/ecs/EntityManager.hpp>
#include <scene/ecs/EntityTag.hpp>

#include <scene/ecs/components/MeshComponent.hpp>
#include <scene/ecs/components/TransformComponent.hpp>
#include <scene/ecs/components/BoundingBoxComponent.hpp>
#include <scene/ecs/components/VisibilityStateComponent.hpp>
#include <scene/ecs/components/LightmapVolumeComponent.hpp>
#include <scene/ecs/components/ShadowMapComponent.hpp>
#include <scene/ecs/components/ReflectionProbeComponent.hpp>
#include <scene/ecs/components/SkyComponent.hpp>

#include <rendering/RenderView.hpp>
#include <rendering/RenderCamera.hpp>
#include <rendering/RenderEnvGrid.hpp>
#include <rendering/RenderEnvProbe.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/subsystems/sky/SkydomeRenderer.hpp>
#include <rendering/RenderBackend.hpp>

#include <core/object/HypClass.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

// #define HYP_DISABLE_VISIBILITY_CHECK
// #define HYP_VISIBILITY_CHECK_DEBUG

namespace hyperion {

#pragma region ViewOutputTarget

ViewOutputTarget::ViewOutputTarget()
{
}

ViewOutputTarget::ViewOutputTarget(const FramebufferRef& framebuffer)
    : m_impl(framebuffer)
{
}

ViewOutputTarget::ViewOutputTarget(GBuffer* gbuffer)
    : m_impl(gbuffer)
{
    Assert(gbuffer != nullptr);
}

GBuffer* ViewOutputTarget::GetGBuffer() const
{
    GBuffer* const* ptr = m_impl.TryGet<GBuffer*>();

    return ptr ? *ptr : nullptr;
}

const FramebufferRef& ViewOutputTarget::GetFramebuffer() const
{
    if (FramebufferRef const* framebuffer = m_impl.TryGet<FramebufferRef>())
    {
        AssertDebug(framebuffer->IsValid());

        return *framebuffer;
    }
    else if (GBuffer* const* gbuffer = m_impl.TryGet<GBuffer*>())
    {
        AssertDebug(*gbuffer != nullptr);

        return (*gbuffer)->GetBucket(RB_OPAQUE).GetFramebuffer();
    }

    return FramebufferRef::Null();
}

const FramebufferRef& ViewOutputTarget::GetFramebuffer(RenderBucket rb) const
{
    if (m_impl.Is<GBuffer*>())
    {
        return m_impl.Get<GBuffer*>()->GetBucket(rb).GetFramebuffer();
    }

    return m_impl.Get<FramebufferRef>();
}

Span<const FramebufferRef> ViewOutputTarget::GetFramebuffers() const
{
    if (!m_impl.IsValid())
    {
        return {};
    }

    if (m_impl.Is<GBuffer*>())
    {
        return m_impl.Get<GBuffer*>()->GetFramebuffers();
    }

    return { &m_impl.Get<FramebufferRef>(), 1 };
}

#pragma endregion ViewOutputTarget

#pragma region View

View::View()
    : View(ViewDesc {})
{
}

View::View(const ViewDesc& viewDesc)
    : m_renderResource(nullptr),
      m_viewDesc(viewDesc),
      m_flags(viewDesc.flags),
      m_viewport(viewDesc.viewport),
      m_scenes(viewDesc.scenes),
      m_camera(viewDesc.camera),
      m_priority(viewDesc.priority),
      m_overrideAttributes(viewDesc.overrideAttributes)
{
    for (auto it = std::begin(m_renderProxyLists); it != std::end(m_renderProxyLists); ++it)
    {
        if ((m_flags & ViewFlags::NOT_MULTI_BUFFERED) && it != std::begin(m_renderProxyLists))
        {
            *it = *(it - 1);

            continue;
        }

        *it = new RenderProxyList;
    }
}

View::~View()
{
    if (GBuffer* gbuffer = m_outputTarget.GetGBuffer())
    {
        delete gbuffer;

        m_outputTarget = ViewOutputTarget();
    }
    else if (FramebufferRef framebuffer = m_outputTarget.GetFramebuffer())
    {
        m_outputTarget = ViewOutputTarget();

        SafeRelease(std::move(framebuffer));
    }

    for (auto it = std::begin(m_renderProxyLists); it != std::end(m_renderProxyLists); ++it)
    {
        // if render proxy lists aren't unique, we just delete the first one and break the loop
        if (it != std::begin(m_renderProxyLists) && *it == *(it - 1))
        {
            break;
        }

        delete *it;
    }

    if (m_renderResource)
    {
        // temp shit
        m_renderResource->DecRef();
        FreeResource(m_renderResource);
        m_renderResource = nullptr;
    }
}

void View::Init()
{
    AddDelegateHandler(g_engine->GetDelegates().OnShutdown.Bind([this]()
        {
            if (m_renderResource)
            {
                if (m_flags & ViewFlags::GBUFFER)
                {
                    m_renderResource->DecRef();
                }

                FreeResource(m_renderResource);
                m_renderResource = nullptr;
            }
        }));

    Assert(m_camera.IsValid(), "Camera is not valid for View with Id #%u", Id().Value());
    InitObject(m_camera);

    const Vec2u extent = MathUtil::Max(m_viewDesc.outputTargetDesc.extent, Vec2u::One());

    if (m_viewDesc.flags & ViewFlags::GBUFFER)
    {
        AssertDebug(m_viewDesc.outputTargetDesc.attachments.Empty(),
            "View with GBuffer flag cannot have output target attachments defined, as it will use GBuffer instead.");

        m_outputTarget = ViewOutputTarget(new GBuffer(extent));
    }
    else if (m_viewDesc.outputTargetDesc.attachments.Any())
    {
        FramebufferRef framebuffer = g_renderBackend->MakeFramebuffer(extent, m_viewDesc.outputTargetDesc.numViews);

        for (uint32 attachmentIndex = 0; attachmentIndex < m_viewDesc.outputTargetDesc.attachments.Size(); ++attachmentIndex)
        {
            const ViewOutputTargetAttachmentDesc& attachmentDesc = m_viewDesc.outputTargetDesc.attachments[attachmentIndex];

            AttachmentRef attachment = framebuffer->AddAttachment(
                attachmentIndex,
                attachmentDesc.format,
                attachmentDesc.imageType,
                attachmentDesc.loadOp,
                attachmentDesc.storeOp);

            attachment->SetClearColor(attachmentDesc.clearColor);
        }

        DeferCreate(framebuffer);

        m_outputTarget = ViewOutputTarget(framebuffer);
    }

    Assert(m_outputTarget.IsValid(), "View with id #%u must have a valid output target!", Id().Value());

    m_renderResource = AllocateResource<RenderView>(this);
    m_renderResource->SetViewport(m_viewport);

    // temp shit
    m_renderResource->IncRef();

    AssertDebug(m_outputTarget.IsValid());

    SetReady(true);
}

bool View::TestRay(const Ray& ray, RayTestResults& outResults, bool useBvh) const
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);

    bool hasHits = false;

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());

        if (scene->GetOctree().TestRay(ray, outResults, useBvh))
        {
            hasHits = true;
        }
    }

    return hasHits;
}

void View::UpdateVisibility()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread);
    AssertReady();

    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with Id #%u, cannot update visibility!", Id().Value());
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        scene->GetOctree().CalculateVisibility(m_camera);
    }
}

void View::Collect()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    RenderProxyList& rpl = RenderApi_GetProducerProxyList(this);
    rpl.BeginWrite();

    // rpl.meshes.Advance();
    // rpl.envProbes.Advance();
    // rpl.envGrids.Advance();
    // rpl.lights.Advance();
    // rpl.lightmapVolumes.Advance();
    // rpl.materials.Advance();
    // rpl.textures.Advance();
    // rpl.skeletons.Advance();

    rpl.viewport = m_viewport;
    rpl.priority = m_priority;

    CollectLights(rpl);
    CollectLightmapVolumes(rpl);
    CollectEnvGrids(rpl);
    CollectEnvProbes(rpl);

    m_lastMeshCollectionResult = CollectMeshEntities(rpl);

    /// temp
    constexpr uint32 bucketMask = (1 << RB_OPAQUE)
        | (1 << RB_LIGHTMAP)
        | (1 << RB_SKYBOX)
        | (1 << RB_TRANSLUCENT)
        | (1 << RB_DEBUG);

    UpdateRefs(rpl);

    rpl.EndWrite();
}

void View::SetViewport(const Viewport& viewport)
{
    m_viewport = viewport;

    if (IsInitCalled())
    {
        m_renderResource->SetViewport(viewport);
    }
}

void View::SetPriority(int priority)
{
    m_priority = priority;

    if (IsInitCalled())
    {
        m_renderResource->SetPriority(priority);
    }
}

void View::AddScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (m_scenes.Contains(scene))
    {
        return;
    }

    m_scenes.PushBack(scene);

    if (IsInitCalled())
    {
        InitObject(scene);
    }
}

void View::RemoveScene(const Handle<Scene>& scene)
{
    HYP_SCOPE;

    if (!scene.IsValid())
    {
        return;
    }

    if (!m_scenes.Contains(scene))
    {
        return;
    }

    m_scenes.Erase(scene);
}

ResourceTrackerDiff View::CollectMeshEntities(RenderProxyList& rpl)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_gameThread | ThreadCategory::THREAD_CATEGORY_TASK);
    AssertReady();

    if (!m_camera.IsValid())
    {
        HYP_LOG(Scene, Warning, "Camera is not valid for View with Id #%u, cannot collect entities!", Id().Value());

        return rpl.meshes.GetDiff();
    }

    const ObjId<Camera> cameraId = m_camera->Id();

    const bool skipFrustumCulling = (m_flags & ViewFlags::NO_FRUSTUM_CULLING);

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        if (scene->GetFlags() & SceneFlags::DETACHED)
        {
            HYP_LOG(Scene, Warning, "Scene \"{}\" has DETACHED flag set, cannot collect entities for render collector!", scene->GetName());

            continue;
        }

        const VisibilityStateSnapshot visibilityStateSnapshot = scene->GetOctree().GetVisibilityState().GetSnapshot(cameraId);

        uint32 numCollectedEntities = 0;
        uint32 numSkippedEntities = 0;

        switch (uint32(m_flags) & uint32(ViewFlags::COLLECT_ALL_ENTITIES))
        {
        case uint32(ViewFlags::COLLECT_ALL_ENTITIES):
            for (auto [entity, meshComponent, visibilityStateComponent] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
            {
                if (!skipFrustumCulling && !(visibilityStateComponent.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
                {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                    if (!visibilityStateComponent.visibilityState)
                    {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                        ++numSkippedEntities;
#endif
                        continue;
                    }

                    if (!visibilityStateComponent.visibilityState->GetSnapshot(cameraId).ValidToParent(visibilityStateSnapshot))
                    {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                        ++numSkippedEntities;
#endif

                        continue;
                    }
#endif
                }

                ++numCollectedEntities;

                rpl.meshes.Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                if (const Handle<Material>& material = meshComponent.material)
                {
                    rpl.materials.Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ true);

                    for (const auto& it : material->GetTextures())
                    {
                        const Handle<Texture>& texture = it.second;

                        if (!texture.IsValid())
                        {
                            continue;
                        }

                        rpl.textures.Track(texture.Id(), texture.Get());
                    }
                }

                if (const Handle<Skeleton>& skeleton = meshComponent.skeleton)
                {
                    rpl.skeletons.Track(skeleton.Id(), skeleton.Get(), skeleton->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ true);
                }
            }

            break;

        case uint32(ViewFlags::COLLECT_STATIC_ENTITIES):
            for (auto [entity, meshComponent, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::STATIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
            {
                if (!skipFrustumCulling && !(visibilityStateComponent.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
                {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                    if (!visibilityStateComponent.visibilityState)
                    {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                        ++numSkippedEntities;
#endif
                        continue;
                    }

                    if (!visibilityStateComponent.visibilityState->GetSnapshot(cameraId).ValidToParent(visibilityStateSnapshot))
                    {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                        ++numSkippedEntities;
#endif

                        continue;
                    }
#endif
                }

                ++numCollectedEntities;

                rpl.meshes.Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                if (const Handle<Material>& material = meshComponent.material)
                {
                    rpl.materials.Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ true);

                    for (const auto& it : material->GetTextures())
                    {
                        const Handle<Texture>& texture = it.second;

                        if (!texture.IsValid())
                        {
                            continue;
                        }

                        rpl.textures.Track(texture.Id(), texture.Get());
                    }
                }

                if (const Handle<Skeleton>& skeleton = meshComponent.skeleton)
                {
                    rpl.skeletons.Track(skeleton.Id(), skeleton.Get(), skeleton->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ true);
                }
            }

            break;

        case uint32(ViewFlags::COLLECT_DYNAMIC_ENTITIES):
            for (auto [entity, meshComponent, visibilityStateComponent, _] : scene->GetEntityManager()->GetEntitySet<MeshComponent, VisibilityStateComponent, EntityTagComponent<EntityTag::DYNAMIC>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
            {
                if (!skipFrustumCulling && !(visibilityStateComponent.flags & VISIBILITY_STATE_FLAG_ALWAYS_VISIBLE))
                {
#ifndef HYP_DISABLE_VISIBILITY_CHECK
                    if (!visibilityStateComponent.visibilityState)
                    {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                        ++numSkippedEntities;
#endif
                        continue;
                    }

                    if (!visibilityStateComponent.visibilityState->GetSnapshot(cameraId).ValidToParent(visibilityStateSnapshot))
                    {
#ifdef HYP_VISIBILITY_CHECK_DEBUG
                        ++numSkippedEntities;
#endif

                        continue;
                    }
#endif
                }

                ++numCollectedEntities;

                rpl.meshes.Track(entity->Id(), entity, entity->GetRenderProxyVersionPtr());

                if (const Handle<Material>& material = meshComponent.material)
                {
                    rpl.materials.Track(material.Id(), material.Get(), material->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ true);

                    for (const auto& it : material->GetTextures())
                    {
                        const Handle<Texture>& texture = it.second;

                        if (!texture.IsValid())
                        {
                            continue;
                        }

                        rpl.textures.Track(texture.Id(), texture.Get());
                    }
                }

                if (const Handle<Skeleton>& skeleton = meshComponent.skeleton)
                {
                    rpl.skeletons.Track(skeleton.Id(), skeleton.Get(), skeleton->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ true);
                }
            }

            break;
        default:
            break;
        }

#ifdef HYP_VISIBILITY_CHECK_DEBUG
        HYP_LOG(Scene, Debug, "Collected {} entities for View {}, {} skipped", numCollectedEntities, Id(), numSkippedEntities);
#endif
    }

    ResourceTrackerDiff meshesDiff = rpl.meshes.GetDiff();

    if (meshesDiff.NeedsUpdate())
    {
        Array<Entity*> added;
        rpl.meshes.GetAdded(added, /* includeChanged */ true);

        for (Entity* entity : added)
        {
            auto&& [meshComponent, transformComponent, boundingBoxComponent] = entity->GetEntityManager()->TryGetComponents<MeshComponent, TransformComponent, BoundingBoxComponent>(entity);
            AssertDebug(meshComponent != nullptr);

            RenderProxyMesh& meshProxy = *rpl.meshes.SetProxy(entity->Id(), RenderProxyMesh());
            meshProxy.entity = entity->WeakHandleFromThis();
            meshProxy.mesh = meshComponent->mesh;
            meshProxy.material = meshComponent->material;
            meshProxy.skeleton = meshComponent->skeleton;
            meshProxy.instanceData = meshComponent->instanceData;
            meshProxy.version = *entity->GetRenderProxyVersionPtr();
            meshProxy.bufferData.modelMatrix = transformComponent ? transformComponent->transform.GetMatrix() : Matrix4::Identity();
            meshProxy.bufferData.previousModelMatrix = meshComponent->previousModelMatrix;
            meshProxy.bufferData.worldAabbMax = boundingBoxComponent ? boundingBoxComponent->worldAabb.max : MathUtil::MinSafeValue<Vec3f>();
            meshProxy.bufferData.worldAabbMin = boundingBoxComponent ? boundingBoxComponent->worldAabb.min : MathUtil::MaxSafeValue<Vec3f>();
            meshProxy.bufferData.userData = reinterpret_cast<EntityShaderData::EntityUserData&>(meshComponent->userData);
        }
    }

    return meshesDiff;
}

void View::CollectLights(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_LIGHTS)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<Light>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            AssertDebug(entity->IsA<Light>());

            Light* light = static_cast<Light*>(entity);

            bool isLightInFrustum = false;

            switch (light->GetLightType())
            {
            case LT_DIRECTIONAL:
                isLightInFrustum = true;
                break;
            case LT_POINT:
                isLightInFrustum = m_camera->GetFrustum().ContainsBoundingSphere(light->GetBoundingSphere());
                break;
            case LT_SPOT:
                // @TODO Implement frustum culling for spot lights
                isLightInFrustum = true;
                break;
            case LT_AREA_RECT:
                isLightInFrustum = m_camera->GetFrustum().ContainsAABB(light->GetAABB());
                break;
            default:
                break;
            }

            if (isLightInFrustum)
            {
                HYP_LOG(Scene, Debug, "Collecting light {} of type {} in view {}", light->Id(), light->InstanceClass()->GetName(), Id());
                rpl.lights.Track(light->Id(), light, light->GetRenderProxyVersionPtr());

                if (light->GetMaterial().IsValid())
                {
                    rpl.materials.Track(light->GetMaterial()->Id(), light->GetMaterial().Get());

                    for (const auto& it : light->GetMaterial()->GetTextures())
                    {
                        const Handle<Texture>& texture = it.second;

                        if (!texture.IsValid())
                        {
                            continue;
                        }

                        rpl.textures.Track(texture->Id(), texture.Get());
                    }
                }
            }
        }
    }
}

void View::CollectLightmapVolumes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_LIGHTMAP_VOLUMES)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, lightmapVolumeComponent] : scene->GetEntityManager()->GetEntitySet<LightmapVolumeComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (!lightmapVolumeComponent.volume.IsValid())
            {
                continue;
            }

            const BoundingBox& volumeAabb = lightmapVolumeComponent.volume->GetAABB();

            if (!volumeAabb.IsValid() || !volumeAabb.IsFinite())
            {
                HYP_LOG(Scene, Warning, "Lightmap volume {} has an invalid AABB in view {}", lightmapVolumeComponent.volume->Id(), Id());

                continue;
            }

            if (!m_camera->GetFrustum().ContainsAABB(volumeAabb))
            {
                continue;
            }

            rpl.lightmapVolumes.Track(lightmapVolumeComponent.volume->Id(), lightmapVolumeComponent.volume,
                lightmapVolumeComponent.volume->GetRenderProxyVersionPtr());
        }
    }
}

void View::CollectEnvGrids(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_ENV_GRIDS)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvGrid>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            AssertDebug(entity->IsA<EnvGrid>());

            EnvGrid* envGrid = static_cast<EnvGrid*>(entity);

            const BoundingBox& gridAabb = envGrid->GetAABB();

            if (!gridAabb.IsValid() || !gridAabb.IsFinite())
            {
                HYP_LOG(Scene, Warning, "EnvGrid {} has an invalid AABB in view {}", envGrid->Id(), Id());

                continue;
            }

            if (!m_camera->GetFrustum().ContainsAABB(gridAabb))
            {
                HYP_LOG(Scene, Debug, "EnvGrid {} is not in frustum of View {}", envGrid->Id(), Id());

                continue;
            }

            rpl.envGrids.Track(envGrid->Id(), envGrid, envGrid->GetRenderProxyVersionPtr());
        }
    }
}

void View::CollectEnvProbes(RenderProxyList& rpl)
{
    HYP_SCOPE;

    if (m_flags & ViewFlags::SKIP_ENV_PROBES)
    {
        return;
    }

    for (const Handle<Scene>& scene : m_scenes)
    {
        Assert(scene.IsValid());
        Assert(scene->IsReady());

        for (auto [entity, _] : scene->GetEntityManager()->GetEntitySet<EntityType<EnvProbe>>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            EnvProbe* probe = static_cast<EnvProbe*>(entity);

            if (!probe->IsSkyProbe())
            {
                const BoundingBox& probeAabb = probe->GetAABB();

                if (!probeAabb.IsValid() || !probeAabb.IsFinite())
                {
                    HYP_LOG(Scene, Warning, "EnvProbe {} has an invalid AABB in view {}", probe->Id(), Id());

                    continue;
                }

                if (!(m_flags & ViewFlags::NO_FRUSTUM_CULLING) && !m_camera->GetFrustum().ContainsAABB(probeAabb))
                {
                    continue;
                }
            }

            rpl.envProbes.Track(probe->Id(), probe, probe->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ false);
        }

        // TEMP SHIT
        for (auto [entity, skyComponent] : scene->GetEntityManager()->GetEntitySet<SkyComponent>().GetScopedView(DataAccessFlags::ACCESS_READ, HYP_FUNCTION_NAME_LIT))
        {
            if (skyComponent.subsystem)
            {
                AssertDebug(skyComponent.subsystem->GetEnvProbe()->IsA<SkyProbe>());

                rpl.envProbes.Track(skyComponent.subsystem->GetEnvProbe()->Id(), skyComponent.subsystem->GetEnvProbe(), skyComponent.subsystem->GetEnvProbe()->GetRenderProxyVersionPtr(), /* allowDuplicatesInSameFrame */ false);
            }
        }
    }

    /// TODO: point light Shadow maps
}

#pragma endregion View

} // namespace hyperion
