/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Scene/LightmapVolume.hpp>
#include <Scene/World.hpp>
#include <Scene/Scene.hpp>
#include <Scene/EntityManager.hpp>

#include <Scene/Systems/LightmapSystem.hpp>
#include <Scene/Systems/LayerOverrideSystem.hpp>

#include <Scene/Layer.hpp>

#include <Scene/Components/LightmapElementComponent.hpp>
#include <Scene/Components/BoundingBoxComponent.hpp>

#include <Rendering/Texture.hpp>
#include <Rendering/RenderProxy.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/Assets.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Utilities/DeferredScope.hpp>

#include <Core/Threading/Threads.hpp>

#include <Framework/EngineDriver.hpp>
#include <Framework/EngineGlobals.hpp>

#ifdef HYP_EDITOR
#include <Baking/Baker.hpp>
#include <Baking/BakerSubsystem.hpp>
#include <Baking/LightmapVolume/LightmapVolumeBakeData.hpp>
#endif // HYP_EDITOR

#include <LightmapVolume.generated.inl>

namespace Hyperion {

#ifdef HYP_EDITOR
EDITOR_API HYP_DECLARE_LOG_CHANNEL(Editor);
#endif // HYP_EDITOR

namespace {

LayerOverrideSystem* GetLayerOverrideSystem(const LightmapVolume* volume)
{
    World* world = volume->GetWorld();

    return world ? world->GetSystem<LayerOverrideSystem>() : nullptr;
}

Name BuildAtlasTextureName(Name volumeName, Name layerName, uint16 atlasIndex, LightmapVolume::AtlasTextureType type)
{
    return NAME_FMT("LightmapVolumeAtlasTexture_{}_{}_{}_{}",
        volumeName, layerName, atlasIndex, LightmapVolume::TextureTypeNames[type]);
}

} // namespace

LightmapVolume::LightmapVolume()
    : LightmapVolume(BoundingBox::Empty())
{
}

LightmapVolume::LightmapVolume(const BoundingBox& localBounds)
    : VolumeBase(localBounds),
      m_irradianceAtlasTextures {},
      m_bentNormalAtlasTextures {},
      m_id(InvalidId)
{
    m_atlases.Reserve(MaxAtlasesPerLightmapVolume);
    m_atlases.EmplaceBack(DefaultAtlasDimensions);
}

LightmapVolume::~LightmapVolume()
{
    if (AnyOf(m_irradianceAtlasTextures, &Handle<Texture>::IsValid))
    {
        EnqueueDeletion(std::move(m_irradianceAtlasTextures));
    }

    if (AnyOf(m_bentNormalAtlasTextures, &Handle<Texture>::IsValid))
    {
        EnqueueDeletion(std::move(m_bentNormalAtlasTextures));
    }
}

void LightmapVolume::SetName(Name name)
{
    if (name == m_name)
    {
        return;
    }

    Node::SetName(name);
}

void LightmapVolume::SetLightmapVolumeId(LightmapVolumeId id)
{
    if (m_id == id)
    {
        return;
    }

    m_id = id;

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

bool LightmapVolume::AddElement(Vec2u dimensions, LightmapElement*& outElement, bool shrinkToFit, float downscaleLimit)
{
    outElement = nullptr;

    Optional<LightmapVolumeAtlas> tmpAtlas;

    for (uint32 atlasIndex = 0; atlasIndex < MaxAtlasesPerLightmapVolume; atlasIndex++)
    {
        LightmapVolumeAtlas* atlas = nullptr;
        bool isNewAtlas = false;

        if (atlasIndex >= m_atlases.Size())
        {
            atlas = &tmpAtlas.Emplace(DefaultAtlasDimensions);
            isNewAtlas = true;
        }
        else
        {
            atlas = &m_atlases[atlasIndex];
        }

        uint32 elementIndex = ~0u;

        if (atlas->AddElement(dimensions, outElement, elementIndex, shrinkToFit, downscaleLimit))
        {
            AssertDebug(elementIndex < UINT16_MAX);

            outElement->id = LightmapElementId(uint32((atlasIndex << 16) | elementIndex));

            if (isNewAtlas)
            {
                m_atlases.Resize(MathUtil::Max(m_atlases.Size(), atlasIndex + 1));

                m_atlases[atlasIndex] = std::move(*atlas);
            }

            SetNeedsRenderProxyUpdate();

            return true;
        }
    }

    // could not add to any atlas
    return false;
}

const LightmapElement* LightmapVolume::GetElement(LightmapElementId elementId) const
{
    uint16 atlasIndex;
    uint16 elementIndex;
    LightmapElement::GetAtlasAndElementIndex(elementId, atlasIndex, elementIndex);

    if (atlasIndex >= m_atlases.Size())
    {
        return nullptr;
    }

    if (elementIndex >= m_atlases[atlasIndex].elements.Size())
    {
        return nullptr;
    }

    return &m_atlases[atlasIndex].elements[elementIndex];
}

void LightmapVolume::RemoveAllElements(uint32 preserveTextureTypesMask)
{
    ClearLayerAtlasTextureOverrides(preserveTextureTypesMask);

    for (LightmapVolumeAtlas& atlas : m_atlases)
    {
        atlas.Clear();
    }

    Handle<AssetRegistry> assetRegistry = GetCurrentAssetRegistry();
    Assert(assetRegistry.IsValid());

    if (!(preserveTextureTypesMask & (1u << IrradianceTexture)))
    {
        for (Handle<Texture>& texture : m_irradianceAtlasTextures)
        {
            if (!texture.IsValid())
            {
                continue;
            }

            assetRegistry->RemoveAsset(texture);

            EnqueueDeletion(std::move(texture));
        }

        m_irradianceAtlasTextures = {};
    }

    if (!(preserveTextureTypesMask & (1u << BentNormalTexture)))
    {
        for (Handle<Texture>& texture : m_bentNormalAtlasTextures)
        {
            if (!texture.IsValid())
            {
                continue;
            }

            assetRegistry->RemoveAsset(texture);

            EnqueueDeletion(std::move(texture));
        }

        m_bentNormalAtlasTextures = {};
    }

    m_atlases.Clear();
    m_atlases.EmplaceBack(DefaultAtlasDimensions);

    // The packing is gone - the next bake has to generate a new one.
    m_packingHash = 0;

    MarkDirty();
    SetNeedsRenderProxyUpdate();
}

const Handle<Texture>& LightmapVolume::GetAtlasTexture(uint16 atlasIndex, AtlasTextureType type) const
{
    if (atlasIndex >= m_atlases.Size())
    {
        AssertDebug(false, "atlas index out of bounds");

        return Handle<Texture>::Null();
    }

    auto textures = GetAtlasTextures(type);

    return textures[atlasIndex];
}

void LightmapVolume::SetAtlasTexture(uint16 atlasIndex, AtlasTextureType type, const Handle<Texture>& texture)
{
    if (atlasIndex >= m_atlases.Size())
    {
        AssertDebug(false, "atlas index out of bounds");

        return;
    }

    auto& textures = GetAtlasTexturesArray(type);

    if (texture == textures[atlasIndex])
    {
        return;
    }

    SetNeedsRenderProxyUpdate();

    EnqueueDeletion(std::move(textures[atlasIndex]));

    if (!texture.IsValid())
    {
        return;
    }

    textures[atlasIndex] = texture;

    texture->SetName(BuildAtlasTextureName(m_name, g_defaultLayerName, atlasIndex, type));
    GetCurrentAssetRegistry()->PutAssetUnique(texture);
}

Name LightmapVolume::GetAtlasTexturesPropertyName(AtlasTextureType type)
{
    switch (type)
    {
    case IrradianceTexture:
        return NAME("IrradianceAtlasTextures");
    case BentNormalTexture:
        return NAME("BentNormalAtlasTextures");
    default:
        return Name::Invalid();
    }
}

FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> LightmapVolume::GetAtlasTexturesForLayer(AtlasTextureType type, Name layerName) const
{
    FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> baseTextures;

    Span<const Handle<Texture>> currentTextures = GetAtlasTextures(type);

    for (uint32 i = 0; i < MaxAtlasesPerLightmapVolume && i < uint32(currentTextures.Size()); i++)
    {
        baseTextures[i] = currentTextures[i];
    }

    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        return baseTextures;
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        return baseTextures;
    }

    BoxedValue overrideValue;

    if (!layerOverrideSystem->GetLayerOverrideValue(this, layerName, GetAtlasTexturesPropertyName(type), overrideValue))
    {
        return baseTextures;
    }

    using AtlasTextureArray = FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume>;

    if (overrideValue.Is<AtlasTextureArray>())
    {
        return overrideValue.Get<AtlasTextureArray>();
    }

    HYP_LOG(Lightmap, Warning, "Layer override '{}' on LightmapVolume '{}' is not an atlas texture array",
        layerName, m_name);

    return baseTextures;
}

void LightmapVolume::SetAtlasTextureForLayer(uint16 atlasIndex, AtlasTextureType type, const Handle<Texture>& texture, Name layerName)
{
    if (!layerName.IsValid() || IsDefaultLayer(layerName))
    {
        SetAtlasTexture(atlasIndex, type, texture);

        return;
    }

    if (atlasIndex >= m_atlases.Size())
    {
        AssertDebug(false, "atlas index out of bounds");

        return;
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        HYP_LOG(Lightmap, Error, "Cannot assign atlas texture for layer '{}' on LightmapVolume '{}': no LayerOverrideSystem",
            layerName, m_name);

        return;
    }

    FixedArray<Handle<Texture>, MaxAtlasesPerLightmapVolume> textures = GetAtlasTexturesForLayer(type, layerName);

    if (textures[atlasIndex] == texture)
    {
        return;
    }

    textures[atlasIndex] = texture;

    if (texture.IsValid())
    {
        texture->SetName(BuildAtlasTextureName(m_name, layerName, atlasIndex, type));
        GetCurrentAssetRegistry()->PutAssetUnique(texture);
    }

    layerOverrideSystem->AddLayerOverrideSet(this, layerName);
    layerOverrideSystem->SetLayerOverrideValue(this, layerName, GetAtlasTexturesPropertyName(type), BoxedValue(std::move(textures)));

    SetNeedsRenderProxyUpdate();
    MarkDirty();
}

void LightmapVolume::ClearLayerAtlasTextureOverrides(uint32 preserveTextureTypesMask)
{
    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        return;
    }

    for (Name layerName : layerOverrideSystem->GetSetLayerNames(this))
    {
        for (uint32 type = 0; type < NumAtlasTextureTypes; type++)
        {
            if (preserveTextureTypesMask & (1u << type))
            {
                continue;
            }

            layerOverrideSystem->RemoveLayerOverrideValue(this, layerName, GetAtlasTexturesPropertyName(AtlasTextureType(type)));
        }
    }
}

#ifdef HYP_EDITOR
Array<Name> LightmapVolume::GetBakedLayerNames() const
{
    Array<Name> layerNames;

    if (m_irradianceAtlasTextures[0].IsValid())
    {
        layerNames.PushBack(g_defaultLayerName);
    }

    LayerOverrideSystem* layerOverrideSystem = GetLayerOverrideSystem(this);

    if (!layerOverrideSystem)
    {
        return layerNames;
    }

    const Name propertyName = GetAtlasTexturesPropertyName(IrradianceTexture);

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

void LightmapVolume::OnAddedToWorld(World* world)
{
    VolumeBase::OnAddedToWorld(world);
    
    if (LightmapSystem* lightmapSystem = world->GetSystem<LightmapSystem>())
    {
        if (m_id == InvalidId)
        {
            SetLightmapVolumeId(lightmapSystem->AllocateLightmapVolumeId());
        }
        else
        {
        
            lightmapSystem->MarkLightmapVolumeIdUsed(m_id);
        }
    }

    // Claim directly rather than going through LightmapSystem::ResolveVolumeAssignments, since this volume
    // may not be enumerable in the entity set yet.
    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, lightmapElementComponent] : scene->GetEntityManager()->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            if (lightmapElementComponent.lightmapVolume.IsValid())
            {
                continue;
            }

            bool isAssigned = false;

            for (uint32 i = 0; i < lightmapElementComponent.NumLightmapVolumeAssignments(); i++)
            {
                if (lightmapElementComponent.lightmapVolumeAssignments[i] == m_id)
                {
                    isAssigned = true;

                    break;
                }
            }

            if (!isAssigned)
            {
                continue;
            }

            const LightmapElement* lightmapElement = GetElement(lightmapElementComponent.lightmapElementId);

            if (!lightmapElement)
            {
                HYP_LOG(Lightmap, Warning, "Lightmap element with ID {} does not exist in lightmap volume", lightmapElementComponent.lightmapElementId);

                continue;
            }

            if (!GetAtlasTexture(lightmapElement->GetAtlasIndex(), IrradianceTexture).IsValid())
            {
                continue;
            }

            lightmapElementComponent.lightmapVolume = MakeWeakRef(this);

            entity->SetNeedsRenderProxyUpdate();
        }
    }
}

void LightmapVolume::OnRemovedFromWorld(World* world)
{
    VolumeBase::OnRemovedFromWorld(world);

    if (m_id != InvalidId)
    {
        if (LightmapSystem* lightmapSystem = world->GetSystem<LightmapSystem>())
        {
            // Doesn't actually free it to be re-used; as we might want to undo removal from the world.
            lightmapSystem->MarkLightmapVolumeIdFreed(m_id);
        }
    }

    for (Scene* scene : world->GetScenes())
    {
        for (auto [entity, lightmapElementComponent] : scene->GetEntityManager()->GetEntitySet<LightmapElementComponent>().GetScopedView(DataAccessFlags::ACCESS_RW))
        {
            if (lightmapElementComponent.lightmapVolume.GetUnsafe() == this)
            {
                lightmapElementComponent.lightmapVolume.Reset();

                entity->SetNeedsRenderProxyUpdate();
            }
        }
    }

    // Entities that were relying on this volume may still have another assignment to fall back to.
    if (LightmapSystem* lightmapSystem = world->GetSystem<LightmapSystem>())
    {
        lightmapSystem->ResolveVolumeAssignments();
    }
}

void LightmapVolume::UpdateRenderProxy(RenderProxyLightmapVolume* proxy)
{
    proxy->lightmapVolume = this;

    for (uint32 i = 0; i < uint32(m_irradianceAtlasTextures.Size()); i++)
    {
        if (proxy->atlasIrradianceTextures[i] != m_irradianceAtlasTextures[i].Get())
        {
            proxy->atlasIrradianceTextures[i] = m_irradianceAtlasTextures[i].Get();

            if (m_irradianceAtlasTextures[i].IsValid())
            {
                // needs full rebind to ensure the texture is uploaded on the render thread.
                proxy->forceRebind = true;
            }
        }
    }

    for (uint32 i = 0; i < uint32(m_bentNormalAtlasTextures.Size()); i++)
    {
        if (proxy->atlasBentNormalTextures[i] != m_bentNormalAtlasTextures[i].Get())
        {
            proxy->atlasBentNormalTextures[i] = m_bentNormalAtlasTextures[i].Get();

            if (m_bentNormalAtlasTextures[i].IsValid())
            {
                // needs full rebind to ensure the texture is uploaded on the render thread.
                proxy->forceRebind = true;
            }
        }
    }

    proxy->numAtlases = uint32(m_atlases.Size());

    const BoundingBox worldAabb = m_localBounds.IsValid()
        ? (GetWorldMatrix() * m_localBounds)
        : BoundingBox::Empty();

    proxy->worldAabb = worldAabb;

    proxy->transformMatrix = GetWorldMatrix()
        * Mat4f::Translation(m_localBounds.GetCenter())
        * Mat4f::Scaling(m_localBounds.GetExtent() * 0.5f);

    proxy->bufferData.aabbMax = Vec4f(worldAabb.max, 1.0f);
    proxy->bufferData.aabbMin = Vec4f(worldAabb.min, 1.0f);
    proxy->bufferData.textureIndex = ~0u; /// \todo : Set the correct texture index based on the element
}

#ifdef HYP_EDITOR

template <Baking::LightmapShadingType ShadingType>
static void EnqueueBake(LightmapVolume& self)
{
    World* world = self.GetWorld();
    AssertDebug(world != nullptr);

    if (!world)
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: not attached to a World", self.GetName());

        return;
    }

    // Bake only the active layer. The packing and the meshes' UV1 are shared, so baking several layers at once
    // would have them race to write the same volume.
    const Handle<Layer>& layer = world->GetActiveLayer();

    if (!layer.IsValid())
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: could not resolve a target layer for it", self.GetName());

        return;
    }

    if (!self.HasNoLayers() && !self.IsInLayer(layer->layerId))
    {
        HYP_LOG(Editor, Error, "Cannot bake {}: it is not in the active layer '{}'", self.GetName(), layer->name);

        return;
    }

    BakerSubsystem* bakerSubsystem = world->GetSubsystem<BakerSubsystem>();

    if (!bakerSubsystem)
    {
        bakerSubsystem = world->AddSubsystem<BakerSubsystem>();
    }

    bakerSubsystem->EnqueueBake(layer->bakeLayer, MakeStrongRef(&self), (1u << uint32(ShadingType)));
}

void LightmapVolume::BakeLightmap()
{
    EnqueueBake<Baking::LightmapShadingType::LIGHTMAP>(*this);
}

void LightmapVolume::BakeBentNormals()
{
    EnqueueBake<Baking::LightmapShadingType::BENT_NORMAL>(*this);
}

#endif // HYP_EDITOR

} // namespace Hyperion
