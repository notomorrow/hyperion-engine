#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Math/Vector3.hpp>

namespace Hyperion {

// Game types reflected by CodeGen, which also writes their C# bindings (NativeTestGame.Bindings.dll)

HYP_ENUM()
enum class NativeTestRarity : uint8
{
    Common = 0,
    Rare = 2,
    Legendary
};

HYP_CLASS()
class NativeTestPickup : public ObjectBase
{
    HYP_OBJECT_BODY(NativeTestPickup);

public:
    HYP_FIELD(Property = "PickupName", Serialize)
    String pickupName;

    HYP_FIELD(Property = "Quantity", Serialize)
    int32 quantity = 0;

    HYP_METHOD()
    int32 GetDoubledQuantity() const
    {
        return quantity * 2;
    }
};

HYP_CLASS(AssetBucket = "Weapons")
class NativeTestWeaponAsset final : public AssetObject
{
    HYP_OBJECT_BODY(NativeTestWeaponAsset);

public:
    HYP_FIELD(Property = "Damage", Serialize)
    int32 damage = 0;

    HYP_FIELD(Property = "FireRate", Serialize)
    float fireRate = 0.0f;

    HYP_FIELD(Property = "DisplayName", Serialize)
    String displayName;

    HYP_FIELD(Property = "IsAutomatic", Serialize)
    bool isAutomatic = false;

    HYP_FIELD(Property = "MuzzleOffset", Serialize)
    Vec3f muzzleOffset = Vec3f::Zero();

    HYP_FIELD(Property = "Rarity", Serialize)
    NativeTestRarity rarity = NativeTestRarity::Common;

    HYP_FIELD(Property = "Ammo", Serialize)
    Handle<NativeTestPickup> ammo;
};

// no AssetBucket attribute, so the registry puts it in the Misc bucket
HYP_CLASS()
class NativeTestNoteAsset final : public AssetObject
{
    HYP_OBJECT_BODY(NativeTestNoteAsset);

public:
    HYP_FIELD(Property = "Text", Serialize)
    String text;
};

} // namespace Hyperion
