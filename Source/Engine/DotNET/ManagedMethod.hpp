/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <DotNET/ManagedAttribute.hpp>
#include <DotNET/Helpers.hpp>

#include <DotNET/Types.hpp>

#include <Core/Types.hpp>

#include <type_traits>

namespace Hyperion::dotnet {

struct ObjectReference;

class ManagedMethod
{
public:
    ManagedMethod() = default;

    ManagedMethod(ManagedGuid guid, ManagedInvoker invoker)
        : m_guid(guid),
          m_invoker(invoker)
    {
    }

    ManagedMethod(ManagedGuid guid, ManagedInvoker invoker, ManagedAttributeSet&& attributes)
        : m_guid(guid),
          m_invoker(invoker),
          m_attributes(std::move(attributes))
    {
    }

    ManagedMethod(const ManagedMethod& other) = delete;
    ManagedMethod& operator=(const ManagedMethod& other) = delete;

    ManagedMethod(ManagedMethod&& other) noexcept = default;
    ManagedMethod& operator=(ManagedMethod&& other) noexcept = default;

    ~ManagedMethod() = default;

    HYP_FORCE_INLINE ManagedGuid GetGuid() const
    {
        return m_guid;
    }

    HYP_FORCE_INLINE const ManagedInvoker& GetInvoker() const
    {
        return m_invoker;
    }

    HYP_FORCE_INLINE const ManagedAttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    HYP_FORCE_INLINE bool Invoke(ObjectReference* thisObjectReference, const BoxedValue** argsBoxed, BoxedValue* outBoxed) const
    {
        return m_invoker(thisObjectReference, argsBoxed, outBoxed);
    }

private:
    ManagedGuid m_guid;
    ManagedInvoker m_invoker;
    ManagedAttributeSet m_attributes;
};

} // namespace Hyperion::dotnet
