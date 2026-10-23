#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Containers/String.hpp>

#include <Core/Math/Vector3.hpp>

namespace Hyperion {

// Game types registered by hand, the way the template registers its Game class, since CodeGen doesn't run on game code

class NativeTestPickup final : public ObjectBase
{
    HYP_OBJECT_BODY(NativeTestPickup);

public:
    String pickupName;
    int32 quantity = 0;
};

class NativeTestWeaponAsset final : public AssetObject
{
    HYP_OBJECT_BODY(NativeTestWeaponAsset);

public:
    int32 damage = 0;
    float fireRate = 0.0f;
    String displayName;
    bool isAutomatic = false;
    Vec3f muzzleOffset = Vec3f::Zero();
    Handle<NativeTestPickup> ammo;
};

// no assetbucket attribute, so the registry puts it in the Misc bucket
class NativeTestNoteAsset final : public AssetObject
{
    HYP_OBJECT_BODY(NativeTestNoteAsset);

public:
    String text;
};

} // namespace Hyperion
