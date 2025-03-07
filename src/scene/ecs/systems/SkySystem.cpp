/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <scene/ecs/systems/SkySystem.hpp>
#include <scene/ecs/EntityManager.hpp>

#include <scene/Mesh.hpp>
#include <scene/Material.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderScene.hpp>
#include <rendering/subsystems/sky/SkydomeRenderer.hpp>

#include <util/MeshBuilder.hpp>

namespace hyperion {

void SkySystem::OnEntityAdded(const Handle<Entity> &entity)
{
    SystemBase::OnEntityAdded(entity);

    SkyComponent &sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);

    if (!sky_component.render_subsystem) {
        sky_component.render_subsystem = GetScene()->GetRenderResource().GetEnvironment()->AddRenderSubsystem<SkydomeRenderer>(Name::Unique("sky_renderer"));
    }
    
    MeshComponent *mesh_component = GetEntityManager().TryGetComponent<MeshComponent>(entity);

    if (!mesh_component) {
        mesh_component = &GetEntityManager().AddComponent<MeshComponent>(entity, MeshComponent { });
    }

    if (!mesh_component->mesh) {
        mesh_component->mesh = MeshBuilder::Cube();
        // mesh_component.mesh->InvertNormals();
        InitObject(mesh_component->mesh);

        GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
    }

    if (!mesh_component->material) {
        Handle<Material> material = CreateObject<Material>();
        material->SetBucket(Bucket::BUCKET_SKYBOX);
        material->SetTexture(MaterialTextureKey::ALBEDO_MAP, sky_component.render_subsystem->GetCubemap());
        material->SetFaceCullMode(FaceCullMode::FRONT);
        material->SetIsDepthTestEnabled(true);
        material->SetIsDepthWriteEnabled(false);
        material->SetShader(ShaderManager::GetInstance()->GetOrCreate(
            NAME("Skybox"),
            ShaderProperties(mesh_component->mesh->GetVertexAttributes())
        ));

        InitObject(material);

        mesh_component->material = std::move(material);

        GetEntityManager().AddTag<EntityTag::UPDATE_RENDER_PROXY>(entity);
    }
}

void SkySystem::OnEntityRemoved(ID<Entity> entity)
{
    SystemBase::OnEntityRemoved(entity);

    SkyComponent &sky_component = GetEntityManager().GetComponent<SkyComponent>(entity);
    MeshComponent &mesh_component = GetEntityManager().GetComponent<MeshComponent>(entity);

    if (sky_component.render_subsystem) {
        GetScene()->GetRenderResource().GetEnvironment()->RemoveRenderSubsystem<SkydomeRenderer>(sky_component.render_subsystem->GetName());

        sky_component.render_subsystem = nullptr;
    }
}

void SkySystem::Process(GameCounter::TickUnit delta)
{
    // do nothing
}

} // namespace hyperion
