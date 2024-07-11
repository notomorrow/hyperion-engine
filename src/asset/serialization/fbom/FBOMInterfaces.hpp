/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FBOM_INTERFACES_HPP
#define HYPERION_FBOM_INTERFACES_HPP

#include <asset/serialization/fbom/FBOMResult.hpp>
#include <asset/serialization/fbom/FBOMEnums.hpp>

#include <core/utilities/UniqueID.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/containers/String.hpp>

#include <core/Defines.hpp>

#include <HashCode.hpp>

namespace hyperion {
class ByteWriter;
namespace fbom {

class FBOMWriter;
class FBOMReader;

class HYP_API IFBOMSerializable
{
public:
    virtual FBOMResult Visit(UniqueID id, FBOMWriter *writer, ByteWriter *out, EnumFlags<FBOMDataAttributes> attributes = FBOMDataAttributes::NONE) const = 0;
    
    virtual UniqueID GetUniqueID() const = 0;
    virtual HashCode GetHashCode() const = 0;
    virtual String ToString(bool deep = true) const = 0;
};

} // namespace fbom
} // namespace hyperion

#endif