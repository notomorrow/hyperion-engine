/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Util.hpp>

#ifdef HYP_EDITOR
#include <Baking/BakeLayer.hpp>
#endif // HYP_EDITOR

namespace Hyperion {

enum class SwatchId : uint32;
static constexpr SwatchId InvalidSwatchId = Invalid<SwatchId>;

ENGINE_API extern const Name g_defaultSwatchName;

HYP_FORCE_INLINE bool IsDefaultSwatch(Name swatchName)
{
    return swatchName == g_defaultSwatchName;
}

HYP_CLASS()
class ENGINE_API Swatch final : public ObjectBase
{
    HYP_OBJECT_BODY(Swatch);

public:
    HYP_FIELD(Property = "Name", Serialize)
    Name name;

    HYP_FIELD(Property = "SwatchId", Serialize)
    SwatchId swatchId = InvalidSwatchId;

#ifdef HYP_EDITOR
    HYP_FIELD(Property = "BakeLayer", EditorOnly, Serialize)
    Baking::BakeLayer bakeLayer;
#endif // HYP_EDITOR

    Swatch() = default;

    Swatch(Name name, SwatchId swatchId)
        : name(name),
          swatchId(swatchId)
#ifdef HYP_EDITOR
         , bakeLayer(name)
#endif // HYP_EDITOR
    {
    }
};

} // namespace Hyperion
