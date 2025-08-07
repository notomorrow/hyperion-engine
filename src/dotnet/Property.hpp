/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Array.hpp>
#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>

#include <dotnet/Helpers.hpp>
#include <dotnet/Attribute.hpp>

#include <dotnet/interop/ManagedGuid.hpp>

#include <core/Types.hpp>

namespace hyperion::dotnet {

class Object;

class Property
{
public:
    Property() = default;

    Property(ManagedGuid guid)
        : m_guid(guid)
    {
    }

    Property(ManagedGuid guid, AttributeSet&& attributes)
        : m_guid(guid),
          m_attributes(std::move(attributes))
    {
    }

    Property(const Property& other) = delete;
    Property& operator=(const Property& other) = delete;

    Property(Property&& other) noexcept = default;
    Property& operator=(Property&& other) noexcept = default;

    ~Property() = default;

    HYP_FORCE_INLINE ManagedGuid GetGuid() const
    {
        return m_guid;
    }

    HYP_FORCE_INLINE const AttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    template <class ReturnType>
    ReturnType InvokeGetter(const Object* objectPtr)
    {
        HypData returnHypData;
        InvokeGetter_Internal(objectPtr, &returnHypData);

        return std::move(returnHypData.Get<ReturnType>());
    }

    template <class T>
    void InvokeSetter(const Object* objectPtr, T&& value)
    {
        HypData valueHypData(std::forward<T>(value));
        const HypData* valueHypDataPtr = &valueHypData;

        return InvokeSetter_Internal(objectPtr, &valueHypDataPtr);
    }

private:
    void InvokeGetter_Internal(const Object* objectPtr, HypData* outReturnHypData);
    void InvokeSetter_Internal(const Object* objectPtr, const HypData** valueHypData);

    ManagedGuid m_guid;
    AttributeSet m_attributes;
};

} // namespace hyperion::dotnet
