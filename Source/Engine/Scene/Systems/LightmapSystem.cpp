/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <ScenePch.hpp>

#include <Scene/Systems/LightmapSystem.hpp>

#include <Scene/EntityManager.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Entity.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <LightmapSystem.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(Lightmap);

LightmapSystem::LightmapSystem()
    : m_nextLightmapVolumeId(0)
{
}

HYP_NODISCARD LightmapVolumeId LightmapSystem::AllocateLightmapVolumeId()
{
    if (World* world = GetWorld())
    {
        world->MarkDirty();
    }

    uint32 nextIdValue;

    do
    {
        // We don't want to trample over IDs that were used for LightmapVolumes that were removed from the scene.
        nextIdValue = m_nextLightmapVolumeId++;
    }
    while (m_freedLightmapVolumeIds.Contains(nextIdValue));

    return static_cast<LightmapVolumeId>(nextIdValue);
}

void LightmapSystem::OnAddedToWorld(World* world)
{
    // Set LightmapVolumes to a valid ID if they don't have one assigned already.

    for (Scene* scene : world->GetScenes())
    {
        EntityManager* mgr = scene->GetEntityManager();

        if (mgr != nullptr)
        {
            for (auto [lmv] : mgr->GetEntitySet<EntityType<LightmapVolume>>())
            {
                if (lmv->GetLightmapVolumeId() == InvalidLightmapVolumeId)
                {
                    lmv->SetLightmapVolumeId(AllocateLightmapVolumeId());
                }
            }
        }
    }
}

void LightmapSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    LightmapElementComponent& lightmapElementComponent = entity->GetComponent<LightmapElementComponent>();

    Scene* scene = entity->GetScene();
    Assert(scene != nullptr);

    if (!ResolveVolumeForEntity(*scene, *entity, lightmapElementComponent))
    {
        HYP_LOG(Lightmap, Warning, "LightmapElementComponent for Entity {} could not be associated at runtime",
                entity->GetName());
    }
}

void LightmapSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    LightmapElementComponent* lightmapElementComponent = entity->TryGetComponent<LightmapElementComponent>();

    if (lightmapElementComponent != nullptr)
    {
        if (lightmapElementComponent->lightmapVolume.IsValid())
        {
            lightmapElementComponent->lightmapVolume.Reset();
        }

        entity->SetNeedsRenderProxyUpdate();
    }
}

void LightmapSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
}

LightmapVolume* LightmapSystem::ResolveVolume(
    const Array<LightmapVolume*>& candidateVolumes,
    const LightmapElementComponent& lightmapElementComponent) const
{
    // Assignments are kept sorted by how much of the entity each volume covers, so the first usable one wins.
    const uint32 numAssignments = lightmapElementComponent.NumLightmapVolumeAssignments();

    for (uint32 i = 0; i < numAssignments; i++)
    {
        const LightmapVolumeId lightmapVolumeId = lightmapElementComponent.lightmapVolumeAssignments[i];

        if (!IsIdForAliveLightmapVolume(lightmapVolumeId))
        {
            continue;
        }

        for (LightmapVolume* lightmapVolume : candidateVolumes)
        {
            if (lightmapVolume->GetLightmapVolumeId() != lightmapVolumeId)
            {
                continue;
            }

            const LightmapElement* lightmapElement = lightmapVolume->GetElement(lightmapElementComponent.lightmapElementId);

            if (!lightmapElement)
            {
                continue;
            }

            // GetAtlasTexture reads whatever the applied layer put in the field, so a volume with no bake for
            // the current layer is passed over in favour of the next assignment.
            if (!lightmapVolume->GetAtlasTexture(lightmapElement->GetAtlasIndex(), LightmapVolume::IrradianceTexture).IsValid())
            {
                continue;
            }

            return lightmapVolume;
        }
    }

    return nullptr;
}

bool LightmapSystem::ApplyResolvedVolume(
    Entity& srcEntity,
    LightmapElementComponent& lightmapElementComponent,
    LightmapVolume* resolvedVolume)
{
    if (lightmapElementComponent.lightmapVolume.GetUnsafe() != resolvedVolume)
    {
        if (resolvedVolume)
        {
            lightmapElementComponent.lightmapVolume = MakeWeakRef(resolvedVolume);
        }
        else
        {
            lightmapElementComponent.lightmapVolume.Reset();
        }

        srcEntity.SetNeedsRenderProxyUpdate();
    }

    return resolvedVolume != nullptr;
}

Array<LightmapVolume*> LightmapSystem::CollectVolumes(Scene& scene)
{
    Array<LightmapVolume*> volumes;

    EntityManager* mgr = scene.GetEntityManager();

    if (!mgr)
    {
        return volumes;
    }

    for (auto [lightmapVolume] : mgr->GetEntitySet<EntityType<LightmapVolume>>().GetScopedView(GetComponentInfos()))
    {
        volumes.PushBack(lightmapVolume);
    }

    return volumes;
}

bool LightmapSystem::ResolveVolumeForEntity(Scene& scene, Entity& srcEntity, LightmapElementComponent& lightmapElementComponent)
{
    const Array<LightmapVolume*> volumes = CollectVolumes(scene);

    return ApplyResolvedVolume(srcEntity, lightmapElementComponent, ResolveVolume(volumes, lightmapElementComponent));
}

void LightmapSystem::ResolveVolumeAssignments()
{
    World* world = GetWorld();

    if (!world)
    {
        return;
    }

    for (Scene* scene : world->GetScenes())
    {
        EntityManager* mgr = scene->GetEntityManager();

        if (!mgr)
        {
            continue;
        }

        // Collected up front so the LightmapVolume entity set isn't scoped while walking the component set.
        const Array<LightmapVolume*> volumes = CollectVolumes(*scene);

        for (auto [entity, lightmapElementComponent] : mgr->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            ApplyResolvedVolume(*entity, lightmapElementComponent, ResolveVolume(volumes, lightmapElementComponent));
        }
    }
}

} // namespace Hyperion
