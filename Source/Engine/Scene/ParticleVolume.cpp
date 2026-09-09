/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/ParticleVolume.hpp>

#include <Scene/World.hpp>
#include <Scene/Swatch.hpp>

#include <Scene/Systems/SwatchOverrideSystem.hpp>

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

SwatchOverrideSystem* GetSwatchOverrideSystem(const ParticleVolume* volume)
{
    World* world = volume->GetWorld();

    return world ? world->GetSystem<SwatchOverrideSystem>() : nullptr;
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

Name ParticleVolume::BuildParticleTextureName(Name volumeName, Name swatchName)
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        return NAME_FMT("ParticleVolume_{}_ParticleTexture", volumeName);
    }

    return NAME_FMT("ParticleVolume_{}_{}_ParticleTexture", volumeName, swatchName);
}

Name ParticleVolume::BuildParticleMeshName(Name volumeName, Name swatchName)
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        return NAME_FMT("ParticleVolume_{}_ParticleMesh", volumeName);
    }

    return NAME_FMT("ParticleVolume_{}_{}_ParticleMesh", volumeName, swatchName);
}

Handle<Texture> ParticleVolume::GetParticleTextureForSwatch(Name swatchName) const
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        return GetParticleTexture();
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        return GetParticleTexture();
    }

    BoxedValue overrideValue;

    if (!swatchOverrideSystem->GetSwatchOverrideValue(this, swatchName, GetParticleTexturePropertyName(), overrideValue))
    {
        return GetParticleTexture();
    }

    if (overrideValue.Is<Handle<Texture>>())
    {
        return overrideValue.Get<Handle<Texture>>();
    }

    HYP_LOG(Scene, Warning, "Swatch override '{}' on ParticleVolume '{}' is not a texture",
        swatchName, GetName());

    return GetParticleTexture();
}

Handle<Mesh> ParticleVolume::GetParticleMeshForSwatch(Name swatchName) const
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        return GetParticleMesh();
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        return GetParticleMesh();
    }

    BoxedValue overrideValue;

    if (!swatchOverrideSystem->GetSwatchOverrideValue(this, swatchName, GetParticleMeshPropertyName(), overrideValue))
    {
        return GetParticleMesh();
    }

    if (overrideValue.Is<Handle<Mesh>>())
    {
        return overrideValue.Get<Handle<Mesh>>();
    }

    HYP_LOG(Scene, Warning, "Swatch override '{}' on ParticleVolume '{}' is not a mesh",
        swatchName, GetName());

    return GetParticleMesh();
}

void ParticleVolume::SetParticleTextureForSwatch(const Handle<Texture>& particleTexture, Name swatchName)
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        SetParticleTexture(particleTexture);

        return;
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        HYP_LOG(Scene, Error, "Cannot assign particle texture for swatch '{}' on ParticleVolume '{}': no SwatchOverrideSystem",
            swatchName, GetName());

        return;
    }

    if (GetParticleTextureForSwatch(swatchName) == particleTexture)
    {
        return;
    }

    if (particleTexture.IsValid())
    {
        particleTexture->SetName(BuildParticleTextureName(GetName(), swatchName));

        if (!particleTexture->IsTransient())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(particleTexture);
        }
    }

    swatchOverrideSystem->AddSwatchOverrideSet(this, swatchName);
    swatchOverrideSystem->SetSwatchOverrideValue(this, swatchName, GetParticleTexturePropertyName(), BoxedValue(particleTexture));

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void ParticleVolume::SetParticleMeshForSwatch(const Handle<Mesh>& particleMesh, Name swatchName)
{
    if (!swatchName.IsValid() || IsDefaultSwatch(swatchName))
    {
        SetParticleMesh(particleMesh);

        return;
    }

    SwatchOverrideSystem* swatchOverrideSystem = GetSwatchOverrideSystem(this);

    if (!swatchOverrideSystem)
    {
        HYP_LOG(Scene, Error, "Cannot assign particle mesh for swatch '{}' on ParticleVolume '{}': no SwatchOverrideSystem",
            swatchName, GetName());

        return;
    }

    if (GetParticleMeshForSwatch(swatchName) == particleMesh)
    {
        return;
    }

    if (particleMesh.IsValid())
    {
        particleMesh->SetName(BuildParticleMeshName(GetName(), swatchName));

        if (!particleMesh->IsTransient())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(particleMesh);
        }
    }

    swatchOverrideSystem->AddSwatchOverrideSet(this, swatchName);
    swatchOverrideSystem->SetSwatchOverrideValue(this, swatchName, GetParticleMeshPropertyName(), BoxedValue(particleMesh));

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
