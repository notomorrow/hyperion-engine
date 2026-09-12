/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Asset/AssetObject.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/BitField.hpp>

#include <Core/Name/Name.hpp>

#include <Core/Util.hpp>
#include <Core/Constants.hpp>


namespace Hyperion {

enum class LayerId : uint32;
static constexpr LayerId InvalidLayerId = Invalid<LayerId>;

HYP_STRUCT()
struct LayersMask : BitField<MaxLayersPerWorld>
{
    HYP_STRUCT_BODY(LayersMask);
};

HYP_CLASS()
class ENGINE_API Layer final : public ObjectBase
{
    HYP_OBJECT_BODY(Layer);

public:
    HYP_FIELD(Property = "Name", Serialize)
    Name name;

    HYP_FIELD(Property = "LayerId", Serialize)
    LayerId layerId = InvalidLayerId;

    Layer() = default;

    Layer(Name name, LayerId layerId)
        : name(name),
          layerId(layerId)
    {
    }
};

} // namespace Hyperion
