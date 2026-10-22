/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Containers/Array.hpp>
#include <Core/Containers/String.hpp>

#include <Core/Utilities/StringView.hpp>

#include <DotNET/Helpers.hpp>
#include <DotNET/ManagedAttribute.hpp>
#include <DotNET/Types.hpp>

#include <DotNET/Interop/ManagedGuid.hpp>

#include <Core/Types.hpp>

namespace Hyperion::dotnet {

class ManagedObject;

class ManagedProperty
{
public:
    ManagedProperty() = default;

    ManagedProperty(ManagedGuid guid, ManagedInvoker getter, ManagedInvoker setter)
        : m_guid(guid),
          m_getter(getter),
          m_setter(setter)
    {
    }

    ManagedProperty(ManagedGuid guid, ManagedInvoker getter, ManagedInvoker setter, ManagedAttributeSet&& attributes)
        : m_guid(guid),
          m_getter(getter),
          m_setter(setter),
          m_attributes(std::move(attributes))
    {
    }

    ManagedProperty(const ManagedProperty& other) = delete;
    ManagedProperty& operator=(const ManagedProperty& other) = delete;

    ManagedProperty(ManagedProperty&& other) noexcept = default;
    ManagedProperty& operator=(ManagedProperty&& other) noexcept = default;

    ~ManagedProperty() = default;

    HYP_FORCE_INLINE ManagedGuid GetGuid() const
    {
        return m_guid;
    }

    HYP_FORCE_INLINE const ManagedAttributeSet& GetAttributes() const
    {
        return m_attributes;
    }

    template <class ReturnType>
    ReturnType InvokeGetter(const ManagedObject* pManagedObject)
    {
        BoxedValue returnValue;

        if (!InvokeGetter_Internal(pManagedObject, &returnValue) || returnValue.IsNull())
        {
            return ReturnType();
        }

        return std::move(returnValue.Get<ReturnType>());
    }

    template <class T>
    bool InvokeSetter(const ManagedObject* pManagedObject, T&& value)
    {
        BoxedValue returnValue(std::forward<T>(value));
        const BoxedValue* returnValuePtr = &returnValue;

        return InvokeSetter_Internal(pManagedObject, &returnValuePtr);
    }

    HYP_FORCE_INLINE bool CanGet() const
    {
        return m_getter.IsValid();
    }

    HYP_FORCE_INLINE bool CanSet() const
    {
        return m_setter.IsValid();
    }

    bool InvokeGetterBoxed(const ManagedObject* pManagedObject, BoxedValue& outValue)
    {
        return InvokeGetter_Internal(pManagedObject, &outValue);
    }

    bool InvokeSetterBoxed(const ManagedObject* pManagedObject, const BoxedValue& value)
    {
        const BoxedValue* valuePtr = &value;

        return InvokeSetter_Internal(pManagedObject, &valuePtr);
    }

private:
    bool InvokeGetter_Internal(const ManagedObject* pManagedObject, BoxedValue* outBoxed);
    bool InvokeSetter_Internal(const ManagedObject* pManagedObject, const BoxedValue** boxedValue);

    ManagedGuid m_guid;
    ManagedInvoker m_getter;
    ManagedInvoker m_setter;
    ManagedAttributeSet m_attributes;
};

} // namespace Hyperion::dotnet
