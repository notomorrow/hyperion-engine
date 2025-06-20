/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_ARRAY_HPP
#define HYPERION_FBOM_ARRAY_HPP

#include <core/containers/String.hpp>
#include <core/containers/Array.hpp>

#include <core/utilities/UniqueID.hpp>

#include <core/serialization/fbom/FBOMBaseTypes.hpp>
#include <core/serialization/fbom/FBOMData.hpp>
#include <core/serialization/fbom/FBOMInterfaces.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <type_traits>

namespace hyperion {
namespace serialization {

class HYP_API FBOMArray final : public FBOMSerializableBase
{
public:
    FBOMArray(const FBOMType& element_type);
    FBOMArray(const FBOMType& element_type, const Array<FBOMData>& values);
    FBOMArray(const FBOMType& element_type, Array<FBOMData>&& values);
    FBOMArray(const FBOMArray& other);
    FBOMArray& operator=(const FBOMArray& other);
    FBOMArray(FBOMArray&& other) noexcept;
    FBOMArray& operator=(FBOMArray&& other) noexcept;
    virtual ~FBOMArray() override;

    HYP_FORCE_INLINE const FBOMType& GetElementType() const
    {
        return m_element_type;
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

    virtual FBOMResult Visit(UniqueID id, FBOMWriter* writer, ByteWriter* out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const override;

    virtual String ToString(bool deep = true) const override;
    virtual UniqueID GetUniqueID() const override;
    virtual HashCode GetHashCode() const override;

private:
    FBOMType m_element_type;
    Array<FBOMData> m_values;
};

} // namespace serialization
} // namespace hyperion

#endif
