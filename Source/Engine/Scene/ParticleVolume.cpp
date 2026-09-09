/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/ParticleVolume.hpp>

#include <Scene/World.hpp>
#include <Scene/Layer.hpp>

#include <Scene/Systems/LayerOverrideSystem.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Core/Threading/Threads.hpp>

#include <ParticleVolume.generated.inl>

namespace Hyperion {

namespace {

LayerOverrideSystem* GetLayerOverrideSystem(const ParticleVolume* volume)
{
    World* world = volume->GetWorld();

    return world ? world->GetSystem<LayerOverrideSystem>() : nullptr;
}

} // namespace

ParticleVolume::ParticleVolume()
    : ParticleVolume(BoundingBox::Empty())
{
}

ParticleVolume::ParticleVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds)
{
}

ParticleVolume::~ParticleVolume()
{
    EnqueueDeletion(std::move(texture));
    EnqueueDeletion(std::move(mesh));
}

void ParticleVolume::SetParticleTexture(const Handle<Texture>& particleTexture)
{
    if (texture == particleTexture)
    {
        return;
    }

    if (texture.IsValid())
    {
        EnqueueDeletion(std::move(texture));
    }

    texture = particleTexture;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void ParticleVolume::SetParticleMesh(const Handle<Mesh>& particleMesh)
{
    if (mesh == particleMesh)
    {
        return;
    }

    if (mesh.IsValid())
    {
        EnqueueDeletion(std::move(mesh));
    }

    mesh = particleMesh;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

Name ParticleVolume::BuildParticleTextureName(Name volumeName, Name layerName)
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        return NAME_FMT("ParticleVolume_{}_ParticleTexture", volumeName);
    }

    return NAME_FMT("ParticleVolume_{}_{}_ParticleTexture", volumeName, layerName);
}

Name ParticleVolume::BuildParticleMeshName(Name volumeName, Name layerName)
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        return NAME_FMT("ParticleVolume_{}_ParticleMesh", volumeName);
    }

    return NAME_FMT("ParticleVolume_{}_{}_ParticleMesh", volumeName, layerName);
}

Handle<Texture> ParticleVolume::GetParticleTextureForLayer(Name layerName) const
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        return GetParticleTexture();
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        return GetParticleTexture();
    }

    BoxedValue overrideValue;

    if (!layerOverrideSystem->GetLayerOverrideValue(this, layerName, GetParticleTexturePropertyName(), overrideValue))
    {
        return GetParticleTexture();
    }

    if (overrideValue.Is<Handle<Texture>>())
    {
        return overrideValue.Get<Handle<Texture>>();
    }

    HYP_LOG(Scene, Warning, "Layer override '{}' on ParticleVolume '{}' is not a texture",
        layerName, GetName());

    return GetParticleTexture();
}

Handle<Mesh> ParticleVolume::GetParticleMeshForLayer(Name layerName) const
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        return GetParticleMesh();
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        return GetParticleMesh();
    }

    BoxedValue overrideValue;

    if (!layerOverrideSystem->GetLayerOverrideValue(this, layerName, GetParticleMeshPropertyName(), overrideValue))
    {
        return GetParticleMesh();
    }

    if (overrideValue.Is<Handle<Mesh>>())
    {
        return overrideValue.Get<Handle<Mesh>>();
    }

    HYP_LOG(Scene, Warning, "Layer override '{}' on ParticleVolume '{}' is not a mesh",
        layerName, GetName());

    return GetParticleMesh();
}

void ParticleVolume::SetParticleTextureForLayer(const Handle<Texture>& particleTexture, Name layerName)
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        SetParticleTexture(particleTexture);

        return;
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        HYP_LOG(Scene, Error, "Cannot assign particle texture for layer '{}' on ParticleVolume '{}': no LayerOverrideSystem",
            layerName, GetName());

        return;
    }

    if (GetParticleTextureForLayer(layerName) == particleTexture)
    {
        return;
    }

    if (particleTexture.IsValid())
    {
        particleTexture->SetName(BuildParticleTextureName(GetName(), layerName));

        if (!particleTexture->IsTransient())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(particleTexture);
        }
    }

    layerOverrideSystem->AddLayerOverrideSet(this, layerName);
    layerOverrideSystem->SetLayerOverrideValue(this, layerName, GetParticleTexturePropertyName(), BoxedValue(particleTexture));

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void ParticleVolume::SetParticleMeshForLayer(const Handle<Mesh>& particleMesh, Name layerName)
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        SetParticleMesh(particleMesh);

        return;
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        HYP_LOG(Scene, Error, "Cannot assign particle mesh for layer '{}' on ParticleVolume '{}': no LayerOverrideSystem",
            layerName, GetName());

        return;
    }

    if (GetParticleMeshForLayer(layerName) == particleMesh)
    {
        return;
    }

    if (particleMesh.IsValid())
    {
        particleMesh->SetName(BuildParticleMeshName(GetName(), layerName));

        if (!particleMesh->IsTransient())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(particleMesh);
        }
    }

    layerOverrideSystem->AddLayerOverrideSet(this, layerName);
    layerOverrideSystem->SetLayerOverrideValue(this, layerName, GetParticleMeshPropertyName(), BoxedValue(particleMesh));

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void ParticleVolume::UpdateRenderProxy(RenderProxyParticleVolume* proxy)
{
    proxy->particleVolume = this;

    if (proxy->particleTexture != texture)
    {
        // force rebind of proxy, to ensure texture gets upload to gpu
        proxy->forceRebind = true;

        proxy->particleTexture = texture;
    }

    if (proxy->particleMesh != mesh)
    {
        // force rebind of proxy, to ensure texture gets upload to gpu
        proxy->forceRebind = true;

        proxy->particleMesh = mesh;
    }

    proxy->worldAabb = GetWorldBounds();


    const Vec3f boxCenter = GetLocalBounds().GetCenter() + origin;
    const Vec3f boxHalfExtent = GetLocalBounds().GetExtent() * 0.5f;

    proxy->bufferData.transformMatrix = GetWorldMatrix() * Mat4f::Translation(boxCenter) * Mat4f::Scaling(boxHalfExtent);
    proxy->bufferData.startSize = startSize;
    proxy->bufferData.randomness = randomness;
    proxy->bufferData.avgLifespan = lifespan;
    proxy->bufferData.maxParticles = maxParticles;
}

} // namespace Hyperion
