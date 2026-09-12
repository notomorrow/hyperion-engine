/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/Systems/WeaponSystem.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Scene.hpp>
#include <Scene/World.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Framework/Gameplay/Weapon.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <WeaponSystem.generated.inl>

namespace Hyperion {

bool WeaponSystem::ShouldProcessScene(Scene* scene) const
{
    static constexpr EnumFlags<SceneFlags> ExpectedFlags = SceneFlags::FOREGROUND;

    return (scene->GetSceneFlags() & (SceneFlags::UI | SceneFlags::DETACHED | ExpectedFlags)) == ExpectedFlags;
}

void WeaponSystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    WeaponComponent& weaponComponent = entity->GetEntityManager()->GetComponent<WeaponComponent>(entity);

    if (!weaponComponent.weapon.IsValid())
    {
        HYP_LOG(Scene, Warning, "WeaponComponent has no Weapon set!");

        return;
    }

    SetWeaponModel(*entity, *weaponComponent.weapon);
}

void WeaponSystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    WeaponComponent& weaponComponent = entity->GetEntityManager()->GetComponent<WeaponComponent>(entity);

    if (!weaponComponent.weapon.IsValid())
    {
        HYP_LOG(Scene, Warning, "WeaponComponent has no Weapon set!");

        return;
    }

    MeshComponent* meshComponent = entity->TryGetComponent<MeshComponent>();

    if (meshComponent != nullptr)
    {
        // consider it 'ours' and fine to delete component if both mesh & material are the same as the Weapon's
        // (not changed by the user in editor)
        const bool shouldDeleteComponent = meshComponent->mesh == weaponComponent.weapon->GetMesh()
                && meshComponent->material == weaponComponent.weapon->GetMaterial();

        if (meshComponent->mesh.IsValid() && meshComponent->mesh == weaponComponent.weapon->GetMesh())
        {
            EnqueueDeletion(std::move(meshComponent->mesh));
        }

        if (meshComponent->material.IsValid() && meshComponent->material == weaponComponent.weapon->GetMaterial())
        {
            EnqueueDeletion(std::move(meshComponent->material));
        }

        if (shouldDeleteComponent)
        {
            entity->RemoveComponent<MeshComponent>();
        }
    }
}

void WeaponSystem::Process(float delta, Span<Handle<Scene>> scenes)
{
    for (Scene* scene : scenes)
    {
        for (auto&& [entity, weaponComponent, meshComponent] : scene->GetEntityManager()->GetEntitySet<WeaponComponent, MeshComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            // Ensure weapon models are up to date
            if (weaponComponent.weapon.IsValid()
                && (meshComponent.mesh != weaponComponent.weapon->GetMesh() || meshComponent.material != weaponComponent.weapon->GetMaterial()))
            {
                SetWeaponModel(*entity, *weaponComponent.weapon);
            }
        }
    }
}

void WeaponSystem::SetWeaponModel(Entity& entity, Weapon& weapon) const
{
    if (MeshComponent* meshComponent = entity.TryGetComponent<MeshComponent>())
    {
        if (meshComponent->mesh.IsValid())
        {
            EnqueueDeletion(std::move(meshComponent->mesh));
        }

        meshComponent->mesh = weapon.GetMesh();

        if (meshComponent->material.IsValid())
        {
            EnqueueDeletion(std::move(meshComponent->material));
        }

        meshComponent->material = weapon.GetMaterial();
    }
    else
    {
        // add a new MeshComponent to the entity
        entity.AddComponent<MeshComponent>(MeshComponent {
            weapon.GetMesh(),
            weapon.GetMaterial()
        });
    }

    entity.SetNeedsRenderProxyUpdate();
}

} // namespace Hyperion
