#include <Game/NativeTestTypes.hpp>

#include <Core/Reflection/ObjectMacros.hpp>
#include <Core/Reflection/ClassUtils.hpp>
#include <Core/Reflection/ClassRegistry.hpp>

namespace Hyperion {

#pragma region Reflection

const Class* g_clsNativeTestPickup = nullptr;
const Class* g_clsNativeTestWeaponAsset = nullptr;
const Class* g_clsNativeTestNoteAsset = nullptr;

const Class* NativeTestPickup::StaticClass()
{
    return g_clsNativeTestPickup;
}

const Class* NativeTestWeaponAsset::StaticClass()
{
    return g_clsNativeTestWeaponAsset;
}

const Class* NativeTestNoteAsset::StaticClass()
{
    return g_clsNativeTestNoteAsset;
}

// clang-format off

HYP_BEGIN_CLASS(NativeTestPickup, -1, 0, NAME("ObjectBase"))
    Field(NAME("PickupName"), &NativeTestPickup::pickupName, OffsetOf(&NativeTestPickup::pickupName), Span<const ClassAttribute> { { ClassAttribute("property", "PickupName"), ClassAttribute("serialize", true) } }),
    Field(NAME("Quantity"), &NativeTestPickup::quantity, OffsetOf(&NativeTestPickup::quantity), Span<const ClassAttribute> { { ClassAttribute("property", "Quantity"), ClassAttribute("serialize", true) } })
HYP_END_CLASS

HYP_BEGIN_CLASS(NativeTestWeaponAsset, -1, 0, NAME("AssetObject"), ClassAttribute("assetbucket", "Weapons"))
    Field(NAME("Damage"), &NativeTestWeaponAsset::damage, OffsetOf(&NativeTestWeaponAsset::damage), Span<const ClassAttribute> { { ClassAttribute("property", "Damage"), ClassAttribute("serialize", true) } }),
    Field(NAME("FireRate"), &NativeTestWeaponAsset::fireRate, OffsetOf(&NativeTestWeaponAsset::fireRate), Span<const ClassAttribute> { { ClassAttribute("property", "FireRate"), ClassAttribute("serialize", true) } }),
    Field(NAME("DisplayName"), &NativeTestWeaponAsset::displayName, OffsetOf(&NativeTestWeaponAsset::displayName), Span<const ClassAttribute> { { ClassAttribute("property", "DisplayName"), ClassAttribute("serialize", true) } }),
    Field(NAME("IsAutomatic"), &NativeTestWeaponAsset::isAutomatic, OffsetOf(&NativeTestWeaponAsset::isAutomatic), Span<const ClassAttribute> { { ClassAttribute("property", "IsAutomatic"), ClassAttribute("serialize", true) } }),
    Field(NAME("MuzzleOffset"), &NativeTestWeaponAsset::muzzleOffset, OffsetOf(&NativeTestWeaponAsset::muzzleOffset), Span<const ClassAttribute> { { ClassAttribute("property", "MuzzleOffset"), ClassAttribute("serialize", true) } }),
    Field(NAME("Ammo"), &NativeTestWeaponAsset::ammo, OffsetOf(&NativeTestWeaponAsset::ammo), Span<const ClassAttribute> { { ClassAttribute("property", "Ammo"), ClassAttribute("serialize", true) } })
HYP_END_CLASS

HYP_BEGIN_CLASS(NativeTestNoteAsset, -1, 0, NAME("AssetObject"))
    Field(NAME("Text"), &NativeTestNoteAsset::text, OffsetOf(&NativeTestNoteAsset::text), Span<const ClassAttribute> { { ClassAttribute("property", "Text"), ClassAttribute("serialize", true) } })
HYP_END_CLASS

// clang-format on

HYP_REGISTER_STATIC_CLASS(NativeTestPickup);
HYP_REGISTER_STATIC_CLASS(NativeTestWeaponAsset);
HYP_REGISTER_STATIC_CLASS(NativeTestNoteAsset);

#pragma endregion Reflection

} // namespace Hyperion
