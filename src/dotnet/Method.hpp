/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <dotnet/Attribute.hpp>
#include <dotnet/Helpers.hpp>

#include <dotnet/Types.hpp>

#include <Types.hpp>

#include <type_traits>

namespace hyperion::dotnet {

struct ObjectReference;

class Method
{
public:
    Method()
        : m_invokeFptr(nullptr)
    {
    }

    Method(ManagedGuid guid, InvokeMethodFunction invokeFptr)
        : m_guid(guid),
          m_invokeFptr(invokeFptr)
    {
    }

    Method(ManagedGuid guid, InvokeMethodFunction invokeFptr, AttributeSet&& attributes)
        : m_guid(guid),
          m_invokeFptr(invokeFptr),
          m_attributes(std::move(attributes))
    {
    }

    Method(const Method& other) = delete;
    Method& operator=(const Method& other) = delete;

    Method(Method&& other) noexcept = default;
    Method& operator=(Method&& other) noexcept = default;

    ~Method() = default;

    HYP_FORCE_INLINE ManagedGuid GetGuid() const
    {
        return m_guid;
    }

    HYP_FORCE_INLINE InvokeMethodFunction GetFunctionPointer() const
    {
        return m_invokeFptr;
    }

    HYP_FORCE_INLINE const AttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    HYP_FORCE_INLINE void Invoke(ObjectReference* thisObjectReference, const HypData** argsHypData, HypData* outReturnHypData) const
    {
        m_invokeFptr(thisObjectReference, argsHypData, outReturnHypData);
    }

private:
    ManagedGuid m_guid;
    InvokeMethodFunction m_invokeFptr;
    AttributeSet m_attributes;
};

} // namespace hyperion::dotnet
