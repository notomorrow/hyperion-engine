/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/systems/SkySystem.hpp>
#include <scene/EntityManager.hpp>

#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <scene/World.hpp>
#include <rendering/Texture.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/ShaderManager.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/PlaceholderData.hpp>

#include <rendering/subsystems/sky/SkydomeRenderer.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <util/MeshBuilder.hpp>

#include <engine/EngineDriver.hpp>

namespace hyperion {

SkySystem::SkySystem(EntityManager& entityManager)
    : SystemBase(entityManager)
{
}

void SkySystem::OnEntityAdded(Entity* entity)
{
    SystemBase::OnEntityAdded(entity);

    Assert(GetWorld() != nullptr);

    HYP_LOG(Entity, Debug, "Adding sky system for entity: #{}, Scene: {}", entity->Id(), GetScene()->GetName());

    SkyComponent& skyComponent = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent* meshComponent = GetEntityManager().TryGetComponent<MeshComponent>(entity);

    if (skyComponent.subsystem)
    {
        GetWorld()->RemoveSubsystem(skyComponent.subsystem);
    }

    AddRenderSubsystemToEnvironment(GetWorld(), GetEntityManager(), entity, skyComponent, meshComponent);

    GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
}

void SkySystem::OnEntityRemoved(Entity* entity)
{
    SystemBase::OnEntityRemoved(entity);

    Assert(GetWorld() != nullptr);

    HYP_LOG(Entity, Debug, "Removing sky system for entity: #{}, Scene: {}", entity->Id(), GetScene()->GetName());

    SkyComponent& skyComponent = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent& meshComponent = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (skyComponent.subsystem)
    {
        GetWorld()->RemoveSubsystem(skyComponent.subsystem);
    }
}

void SkySystem::Process(float delta)
{
    // /// @TODO: Remove this
    // for (auto [entity, skyComponent] : GetEntityManager().GetEntitySet<SkyComponent>().GetScopedView(GetComponentInfos()))
    // {
    //     if (!skyComponent.renderSubsystem)
    //     {
    //         continue;
    //     }

    //     const Handle<EnvProbe>& envProbe = skyComponent.renderSubsystem->GetEnvProbe();

    //     if (!envProbe.IsValid() || !envProbe->IsReady())
    //     {
    //         continue;
    //     }

    //     // envProbe->Update(delta);
    // }
}

void SkySystem::AddRenderSubsystemToEnvironment(World* world, EntityManager& mgr, Entity* entity, SkyComponent& skyComponent, MeshComponent* meshComponent)
{
    Assert(world != nullptr);

    if (skyComponent.subsystem)
    {
        world->AddSubsystem(skyComponent.subsystem);
    }
    else
    {
        skyComponent.subsystem = world->AddSubsystem<SkydomeRenderer>();

        Handle<Mesh> mesh = meshComponent ? meshComponent->mesh : Handle<Mesh>::empty;
        Handle<Material> material = meshComponent ? meshComponent->material : Handle<Material>::empty;

        if (!mesh.IsValid())
        {
            mesh = MeshBuilder::Cube();
            // meshComponent.mesh->InvertNormals();
            InitObject(mesh);
        }

        if (!material)
        {
            MaterialAttributes materialAttributes {};
            materialAttributes.shaderDefinition = ShaderDefinition {
                NAME("Skybox"),
                ShaderProperties(mesh->GetVertexAttributes())
            };
            materialAttributes.bucket = RB_SKYBOX;
            materialAttributes.cullFaces = FCM_FRONT;
            materialAttributes.flags = MAF_DEPTH_TEST;

            material = CreateObject<Material>(NAME("SkyboxMaterial"), materialAttributes);
            material->SetTexture(MaterialTextureKey::ALBEDO_MAP, ObjCast<SkyProbe>(skyComponent.subsystem->GetEnvProbe())->GetSkyboxCubemap());

            InitObject(material);
        }

        if (meshComponent)
        {
            meshComponent->mesh = std::move(mesh);
            meshComponent->material = std::move(material);

            mgr.AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
        }
        else
        {
            mgr.AddComponent<MeshComponent>(entity, MeshComponent { std::move(mesh), std::move(material) });
        }
    }
}

} // namespace hyperion
