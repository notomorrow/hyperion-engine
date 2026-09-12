/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 * */

#pragma once

#include <Core/Defines.hpp>
#include <Core/Types.hpp>

#include <Asset/AssetObject.hpp>

namespace Hyperion {

class Mesh;
class Material;

HYP_ENUM()
enum class WeaponType : uint8
{
    Melee,
    Ranged,
    Magic,

    Max
};

/// Definition of a Weapon
HYP_CLASS(AssetBucket = "Weapons")
class ENGINE_API Weapon : public AssetObject
{
    HYP_OBJECT_BODY(Weapon);

public:
    Weapon() = default;
    Weapon(Name name, WeaponType type)
        : AssetObject(name),
          m_type(type)
    {
    }

    ~Weapon() override;

    HYP_METHOD(Property = "Type")
    HYP_FORCE_INLINE WeaponType GetWeaponType() const
    {
        return m_type;
    }

    HYP_METHOD(Property = "Mesh")
    HYP_FORCE_INLINE const Handle<Mesh>& GetMesh() const
    {
        return m_mesh;
    }

    HYP_METHOD(Property = "Material")
    HYP_FORCE_INLINE const Handle<Material>& GetMaterial() const
    {
        return m_material;
    }

private:
    /// The type of the weapon.
    HYP_FIELD(Property = "Type", Serialize, Editor)
    WeaponType m_type;

    ///Mesh for the Weapon's model.
    HYP_FIELD(Property = "Mesh", Serialize, Editor)
    Handle<Mesh> m_mesh;

    ///Material for the Weapon's model.
    HYP_FIELD(Property = "Material", Serialize, Editor)
    Handle<Material> m_material;
};

} // namespace Hyperion
