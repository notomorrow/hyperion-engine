/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Framework/EngineDefaults.hpp>

#include <Asset/AssetRegistry.hpp>
#include <Asset/AssetObject.hpp>
#include <Asset/AssetBucket.hpp>

#include <thread>

#include <EngineDefaults.generated.inl>

namespace Hyperion {

static constexpr const char* TerrainLayerNames[][2] = {
    { "Terrain_Layer0_Albedo", "Terrain_Layer0_Normal" },
    { "Terrain_Layer1_Albedo", "Terrain_Layer1_Normal" },
    { "Terrain_Layer2_Albedo", "Terrain_Layer2_Normal" },
    { "Terrain_Layer3_Albedo", "Terrain_Layer3_Normal" }
};

static AssetPath EngineAssetPath(const AssetBucket& bucket, ANSIStringView name)
{
    return AssetPath(AssetRegistryId::Engine, bucket, CreateNameFromDynamicString(name));
}

const EngineDefaults& EngineDefaults::Get()
{
    static EngineDefaults s_engineDefaults;
    static std::once_flag s_onceFlag;

    std::call_once(s_onceFlag, []()
                   {
                       EngineDefaults& defaults = s_engineDefaults;

                       defaults.blueNoise = EngineAssetPath(AssetBuckets::RawData, "BlueNoise");
                       defaults.skyDome = EngineAssetPath(AssetBuckets::Prefabs, "InvSphere");
                       defaults.font = EngineAssetPath(AssetBuckets::FontAtlases, "Roboto_Regular");

                       defaults.terrain.groundCover = EngineAssetPath(AssetBuckets::Terrain, "DefaultGroundCover");

                       for (const auto& layerNames : TerrainLayerNames)
                       {
                           EngineTerrainLayerDefaults& layer = defaults.terrain.layers.EmplaceBack();
                           layer.albedo = EngineAssetPath(AssetBuckets::Textures, layerNames[0]);
                           layer.normal = EngineAssetPath(AssetBuckets::Textures, layerNames[1]);
                       }
                   });

    return s_engineDefaults;
}

Handle<AssetObject> EngineDefaults::LoadAsset(const AssetPath& path)
{
    Handle<AssetRegistry> registry = GetEngineAssetRegistry();

    if (!path.IsValid() || !registry.IsValid())
    {
        return Handle<AssetObject>::empty;
    }

    return registry->GetAsset(path.GetBucket(), path.assetName);
}

} // namespace Hyperion
