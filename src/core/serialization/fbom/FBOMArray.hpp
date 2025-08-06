/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/utilities/UniqueId.hpp>

#include <core/serialization/fbom/FBOMBaseTypes.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMInterfaces.hpp>

#include <core/Types.hpp>
#include <core/Constants.hpp>

#include <type_traits>

namespace hyperion {
namespace serialization {

class HYP_API FBOMArray final : public FBOMSerializableBase
{
public:
    FBOMArray(const FBOMType& elementType);
    FBOMArray(const FBOMType& elementType, const Array<FBOMData>& values);
    FBOMArray(const FBOMType& elementType, Array<FBOMData>&& values);
    FBOMArray(const FBOMArray& other);
    FBOMArray& operator=(const FBOMArray& other);
    FBOMArray(FBOMArray&& other) noexcept;
    FBOMArray& operator=(FBOMArray&& other) noexcept;
    virtual ~FBOMArray() override;

    HYP_FORCE_INLINE const FBOMType& GetElementType() const
    {
        return m_elementType;
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_values.Size();
    }

    FBOMArray& AddElement(const FBOMData& value);
    FBOMArray& AddElement(FBOMData&& value);

    FBOMData& GetElement(SizeType index);
    const FBOMData& GetElement(SizeType index) const;
    const FBOMData* TryGetElement(SizeType index) const;

    FBOMResult Visit(FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const
    {
        return Visit(GetUniqueID(), writer, out, attributes);
    }

    virtual FBOMResult Visit(UniqueId id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    virtual String ToString(bool deep = true) const override;
    virtual UniqueId GetUniqueID() const override;
    virtual HashCode GetHashCode() const override;

private:
    FBOMType m_elementType;
    Array<FBOMData> m_values;
};

} // namespace serialization
} // namespace hyperion
