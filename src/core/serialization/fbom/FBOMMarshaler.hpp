/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/memory/UniquePtr.hpp>
#include <core/memory/Any.hpp>
#include <core/memory/AnyRef.hpp>

#include <core/Util.hpp>

#include <core/serialization/fbom/FBOMType.hpp>
#include <core/serialization/fbom/FBOMResult.hpp>
#include <core/serialization/fbom/FBOMBaseTypes.hpp>

#include <core/Constants.hpp>

namespace hyperion {
struct HypData;
} // namespace hyperion

namespace hyperion::serialization {

class FBOM;
class FBOMObject;
class FBOMLoadContext;

class FBOMMarshalerBase
{
public:
    virtual ~FBOMMarshalerBase() = default;

    virtual FBOMType GetObjectType() const = 0;
    virtual TypeId GetTypeId() const = 0;

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const = 0;
    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const = 0;
};

template <class T>
class FBOMObjectMarshalerBase;

template <class T>
class FBOMMarshaler;

struct FBOMMarshalerRegistrationBase
{
protected:
    FBOMMarshalerRegistrationBase(TypeId typeId, ANSIStringView name, UniquePtr<FBOMMarshalerBase>&& marshal);
};

template <class T, class MarshalerType>
struct FBOMMarshalerRegistration : FBOMMarshalerRegistrationBase
{
    FBOMMarshalerRegistration()
        : FBOMMarshalerRegistrationBase(TypeId::ForType<T>(), TypeNameHelper<T, true>::value.Data(), MakeUnique<MarshalerType>())
    {
    }
};

template <class T>
class FBOMObjectMarshalerBase : public FBOMMarshalerBase
{
public:
    virtual ~FBOMObjectMarshalerBase() = default;

    virtual FBOMType GetObjectType() const override
    {
        return FBOMObjectType(TypeNameHelper<T, true>::value.Data());
    }

    virtual TypeId GetTypeId() const override final
    {
        return TypeId::ForType<T>();
    }

    virtual FBOMResult Serialize(ConstAnyRef in, FBOMObject& out) const override final
    {
        HYP_CORE_ASSERT(in.Is<T>(), "Cannot serialize - given object is not of expected type");

        if (!in.Is<T>())
        {
            return { FBOMResult::FBOM_ERR, "Cannot serialize - given object is not of expected type" };
        }

        return Serialize(in.Get<T>(), out);
    }

    virtual FBOMResult Serialize(const T& in, FBOMObject& out) const = 0;
    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override = 0;
};

template <class T>
class FBOMMarshaler : public FBOMObjectMarshalerBase<T>
{
public:
    virtual FBOMResult Serialize(const T& in, FBOMObject& out) const override;
    virtual FBOMResult Deserialize(FBOMLoadContext& context, const FBOMObject& in, HypData& out) const override;
};

#define HYP_DEFINE_MARSHAL(T, MarshalType)                                                            \
    static ::hyperion::FBOMMarshalerRegistration<T, MarshalType> HYP_UNIQUE_NAME(marshalRegistration) \
    {                                                                                                 \
    }

} // namespace hyperion::serialization
