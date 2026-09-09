/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <ScenePch.hpp>

#include <Scene/FogVolume.hpp>
#include <Scene/Scene.hpp>
#include <Scene/World.hpp>
#include <Scene/Layer.hpp>

#include <Scene/Util/SceneHelpers.hpp>

#include <Scene/Systems/LayerOverrideSystem.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/Shared.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Reflection/Handle.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>

#ifdef HYP_EDITOR
#include <Baking/BakerSubsystem.hpp>
#include <Baking/FogVolume/FogVolumeBakeData.hpp>
#endif

#include <FogVolume.generated.inl>

namespace Hyperion {

#ifdef HYP_EDITOR
EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
#endif // HYP_EDITOR

namespace {

LayerOverrideSystem* GetLayerOverrideSystem(const FogVolume* volume)
{
    World* world = volume->GetWorld();

    return world ? world->GetSystem<LayerOverrideSystem>() : nullptr;
}

} // namespace

FogVolume::FogVolume()
{
}

FogVolume::FogVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds)
{
}

FogVolume::~FogVolume()
{
    if (m_volumeTexture)
    {
        EnqueueDeletion(std::move(m_volumeTexture));
    }

    if (m_noiseTexture)
    {
        EnqueueDeletion(std::move(m_noiseTexture));
    }
}

void FogVolume::SetVolumeTexture(const Handle<Texture>& volumeTexture)
{
    if (m_volumeTexture == volumeTexture)
    {
        return;
    }

    if (m_volumeTexture)
    {
        EnqueueDeletion(std::move(m_volumeTexture));
    }

    m_volumeTexture = volumeTexture;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetNoiseTexture(const Handle<Texture>& noiseTexture)
{
    if (m_noiseTexture == noiseTexture)
    {
        return;
    }

    if (m_noiseTexture)
    {
        EnqueueDeletion(std::move(m_noiseTexture));
    }

    m_noiseTexture = noiseTexture;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void FogVolume::SetTextures(
    const Handle<Texture>& volumeTexture,
    const Handle<Texture>& noiseTexture)
{
    SetVolumeTexture(volumeTexture);
    SetNoiseTexture(noiseTexture);
}

Name FogVolume::BuildVolumeTextureName(Name volumeName, Name layerName)
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        return NAME_FMT("FogVolume_{}_DataMap", volumeName);
    }

    return NAME_FMT("FogVolume_{}_{}_DataMap", volumeName, layerName);
}

Name FogVolume::BuildNoiseTextureName(Name volumeName, Name layerName)
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        return NAME_FMT("FogVolume_{}_NoiseMap", volumeName);
    }

    return NAME_FMT("FogVolume_{}_{}_NoiseMap", volumeName, layerName);
}

Handle<Texture> FogVolume::GetVolumeTextureForLayer(Name layerName) const
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        return GetVolumeTexture();
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        return GetVolumeTexture();
    }

    BoxedValue overrideValue;

    if (!layerOverrideSystem->GetLayerOverrideValue(this, layerName, GetVolumeTexturePropertyName(), overrideValue))
    {
        return GetVolumeTexture();
    }

    if (overrideValue.Is<Handle<Texture>>())
    {
        return overrideValue.Get<Handle<Texture>>();
    }

    HYP_LOG(Scene, Warning, "Layer override '{}' on FogVolume '{}' is not a texture",
        layerName, GetName());

    return GetVolumeTexture();
}

Handle<Texture> FogVolume::GetNoiseTextureForLayer(Name layerName) const
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        return GetNoiseTexture();
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        return GetNoiseTexture();
    }

    BoxedValue overrideValue;

    if (!layerOverrideSystem->GetLayerOverrideValue(this, layerName, GetNoiseTexturePropertyName(), overrideValue))
    {
        return GetNoiseTexture();
    }

    if (overrideValue.Is<Handle<Texture>>())
    {
        return overrideValue.Get<Handle<Texture>>();
    }

    HYP_LOG(Scene, Warning, "Layer override '{}' on FogVolume '{}' is not a texture",
        layerName, GetName());

    return GetNoiseTexture();
}

void FogVolume::SetTexturesForLayer(
    const Handle<Texture>& volumeTexture,
    const Handle<Texture>& noiseTexture,
    Name layerName)
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        SetTextures(volumeTexture, noiseTexture);

        return;
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        HYP_LOG(Scene, Error, "Cannot assign textures for layer '{}' on FogVolume '{}': no LayerOverrideSystem",
            layerName, GetName());

        return;
    }

    if (GetVolumeTextureForLayer(layerName) == volumeTexture
        && GetNoiseTextureForLayer(layerName) == noiseTexture)
    {
        return;
    }

    if (volumeTexture.IsValid())
    {
        volumeTexture->SetName(BuildVolumeTextureName(GetName(), layerName));

        if (!volumeTexture->IsTransient())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(volumeTexture);
        }
    }

    if (noiseTexture.IsValid())
    {
        noiseTexture->SetName(BuildNoiseTextureName(GetName(), layerName));

        if (!noiseTexture->IsTransient())
        {
            GetCurrentAssetRegistry()->PutAssetUnique(noiseTexture);
        }
    }

    layerOverrideSystem->AddLayerOverrideSet(this, layerName);
    layerOverrideSystem->SetLayerOverrideValue(this, layerName, GetVolumeTexturePropertyName(), BoxedValue(volumeTexture));
    layerOverrideSystem->SetLayerOverrideValue(this, layerName, GetNoiseTexturePropertyName(), BoxedValue(noiseTexture));

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

#ifdef HYP_EDITOR

Array<Name> FogVolume::GetBakedLayerNames() const
{
    Array<Name> layerNames;

    if (m_volumeTexture.IsValid())
    {
        layerNames.PushBack(g_defaultLayerName);
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        return layerNames;
    }

    const Name propertyName = GetVolumeTexturePropertyName();

    for (Name layerName : layerOverrideSystem->GetSetLayerNames(this))
    {
        if (layerOverrideSystem->IsPropertyOverriddenInLayer(this, layerName, propertyName))
        {
            layerNames.PushBack(layerName);
        }
    }

    return layerNames;
}

#endif // HYP_EDITOR

void FogVolume::UpdateRenderProxy(RenderProxyFogVolume* proxy)
{
    AssertDebug(proxy != nullptr);

    const BoundingBox worldAabb = GetWorldBounds();

    proxy->fogVolume = this;
    proxy->worldAabb = worldAabb;

    if (proxy->volumeTexture != m_volumeTexture)
    {
        proxy->forceRebind = true;

        proxy->volumeTexture = m_volumeTexture;
    }

    if (proxy->noiseTexture != m_noiseTexture)
    {
        proxy->forceRebind = true;

        proxy->noiseTexture = m_noiseTexture;
    }

    // create transform matrix turning 1:1:1 cube to the world bounds
    const Mat4f newTransformMatrix = GetWorldMatrix()
        * Mat4f::Translation(m_localBounds.GetCenter())
        * Mat4f::Scaling(m_localBounds.GetExtent() * 0.5f);

    if (newTransformMatrix != proxy->bufferData.transformMatrix
        || worldAabb.min != proxy->bufferData.aabbMin.GetXYZ()
        || worldAabb.max != proxy->bufferData.aabbMax.GetXYZ())
    {
        proxy->forceRebind = true;

        proxy->bufferData.transformMatrix = newTransformMatrix;
        proxy->bufferData.aabbMin = Vec4f(worldAabb.min, 1.0f);
        proxy->bufferData.aabbMax = Vec4f(worldAabb.max, 1.0f);
    }
}

#ifdef HYP_EDITOR

void FogVolume::Rebake()
{
    World* world = GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: not attached to a World", Id());

        return;
    }

    // Bake only the active layer
    const Handle<Layer>& layer = world->GetActiveLayer();

    if (!layer.IsValid())
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: could not resolve the active layer", GetName());

        return;
    }

    if (!HasNoLayers() && !IsInLayer(layer->layerId))
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: it is not in the active layer '{}'", GetName(), layer->name);

        return;
    }

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    bakerSubsystem->EnqueueBake(layer->bakeLayer, MakeStrongRef(this));
}

#endif

} // namespace Hyperion
