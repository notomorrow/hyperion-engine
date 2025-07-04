/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_SCRIPTABLE_DELEGATE_HPP
#define HYPERION_SCRIPTABLE_DELEGATE_HPP

#include <core/functional/ScriptableDelegateFwd.hpp>
#include <core/functional/Delegate.hpp>

#include <core/memory/resource/Resource.hpp>

#include <core/object/managed/ManagedObjectResource.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <dotnet/Object.hpp>

namespace hyperion {

namespace functional {

HYP_API void LogScriptableDelegateError(const char* message, dotnet::Object* objectPtr);

class IScriptableDelegate : public virtual IDelegate
{
public:
    virtual ~IScriptableDelegate() = default;

    virtual DelegateHandler BindManaged(const String& methodName, Proc<ManagedObjectResource*()>&& getFn) = 0;
    virtual DelegateHandler BindManaged(const String& methodName, ManagedObjectResource* managedObjectResource) = 0;
    virtual DelegateHandler BindManaged(const String& methodName, UniquePtr<dotnet::Object>&& object) = 0;
};

/*! \brief A delegate that can be bound to a managed .NET object.
 *  \details This delegate can be bound to a managed .NET object, allowing the delegate have its behavior defined in script code.
 *  \tparam ReturnType The return type of the delegate.
 *  \tparam Args The argument types of the delegate.*/
template <class ReturnType, class... Args>
class ScriptableDelegate final : public IScriptableDelegate, public virtual Delegate<ReturnType, Args...>
{
public:
    using ProcType = Proc<ReturnType(Args...)>;

    ScriptableDelegate() = default;
    ScriptableDelegate(const ScriptableDelegate& other) = delete;
    ScriptableDelegate& operator=(const ScriptableDelegate& other) = delete;

    ScriptableDelegate(ScriptableDelegate&& other) noexcept
        : Delegate<ReturnType, Args...>(std::move(other.m_delegate))
    {
    }

    ScriptableDelegate& operator=(ScriptableDelegate&& other) noexcept = delete;

    virtual ~ScriptableDelegate() override = default;

    HYP_NODISCARD virtual DelegateHandler BindManaged(const String& methodName, Proc<ManagedObjectResource*()>&& getFn) override
    {
        if (!getFn)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([methodName = methodName, getFn = std::move(getFn)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                ManagedObjectResource* managedObjectResource = getFn();
                HYP_CORE_ASSERT(managedObjectResource != nullptr, "Managed object resource is null!");

                managedObjectResource->IncRef();

                dotnet::Object* object = managedObjectResource->GetManagedObject();
                HYP_CORE_ASSERT(object != nullptr, "Managed object is null!");
                HYP_CORE_ASSERT(object->IsValid(), "Managed object is invalid!");

                if (!object->GetMethod(methodName))
                {
                    HYP_FAIL("Failed to find method %s!", methodName.Data());
                }

                if constexpr (std::is_void_v<ReturnType>)
                {
                    object->InvokeMethodByName<void>(methodName, std::forward<ArgTypes>(args)...);

                    managedObjectResource->DecRef();
                }
                else
                {
                    ReturnType result = object->InvokeMethodByName<ReturnType>(methodName, std::forward<ArgTypes>(args)...);

                    managedObjectResource->DecRef();

                    return result;
                }
            });
    }

    template <class DefaultReturnType, typename = std::enable_if_t<std::is_copy_constructible_v<NormalizedType<DefaultReturnType>>>>
    HYP_NODISCARD DelegateHandler BindManaged(const String& methodName, Proc<ManagedObjectResource*()>&& getFn, DefaultReturnType&& defaultReturn)
    {
        if (!getFn)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([methodName = methodName, getFn = std::move(getFn), defaultReturn = std::forward<DefaultReturnType>(defaultReturn)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                ManagedObjectResource* managedObjectResource = getFn();
                HYP_CORE_ASSERT(managedObjectResource != nullptr, "Managed object resource is null!");

                managedObjectResource->IncRef();
                HYP_DEFER({ managedObjectResource->DecRef(); });

                dotnet::Object* object = managedObjectResource->GetManagedObject();
                HYP_CORE_ASSERT(object != nullptr, "Managed object is null!");
                HYP_CORE_ASSERT(object->IsValid(), "Managed object is invalid!");

                if (!object->GetMethod(methodName))
                {
                    return defaultReturn;
                }

                return object->InvokeMethodByName<ReturnType>(methodName, std::forward<ArgTypes>(args)...);
            });
    }

    HYP_NODISCARD virtual DelegateHandler BindManaged(const String& methodName, ManagedObjectResource* managedObjectResource) override
    {
        if (!managedObjectResource)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([methodName = methodName, managedObjectResource]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                managedObjectResource->IncRef();
                HYP_DEFER({ managedObjectResource->DecRef(); });

                dotnet::Object* object = managedObjectResource->GetManagedObject();
                HYP_CORE_ASSERT(object != nullptr, "Managed object is null!");
                HYP_CORE_ASSERT(object->IsValid(), "Managed object is invalid!");

                if (!object->GetMethod(methodName))
                {
                    HYP_FAIL("Failed to find method %s!", methodName.Data());
                }

                return object->InvokeMethodByName<ReturnType>(methodName, std::forward<ArgTypes>(args)...);
            });
    }

    template <class DefaultReturnType, typename = std::enable_if_t<std::is_copy_constructible_v<NormalizedType<DefaultReturnType>>>>
    HYP_NODISCARD DelegateHandler BindManaged(const String& methodName, ManagedObjectResource* managedObjectResource, DefaultReturnType&& defaultReturn)
    {
        if (!managedObjectResource)
        {
            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([methodName = methodName, managedObjectResource, defaultReturn = std::forward<DefaultReturnType>(defaultReturn)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                managedObjectResource->IncRef();
                HYP_DEFER({ managedObjectResource->DecRef(); });

                dotnet::Object* object = managedObjectResource->GetManagedObject();
                HYP_CORE_ASSERT(object != nullptr, "Managed object is null!");
                HYP_CORE_ASSERT(object->IsValid(), "Managed object is invalid!");

                if (!object->GetMethod(methodName))
                {
                    return defaultReturn;
                }

                return object->InvokeMethodByName<ReturnType>(methodName, std::forward<ArgTypes>(args)...);
            });
    }

    HYP_NODISCARD virtual DelegateHandler BindManaged(const String& methodName, UniquePtr<dotnet::Object>&& object) override
    {
        if (!object)
        {
            return DelegateHandler();
        }

        if (!object->IsValid())
        {
            LogScriptableDelegateError("Managed object is invalid!", object.Get());

            return DelegateHandler();
        }

        if (!object->SetKeepAlive(true))
        {
            LogScriptableDelegateError("Failed to set keep alive to true!", object.Get());

            return DelegateHandler();
        }

        if (!object->GetMethod(methodName))
        {
            LogScriptableDelegateError("Failed to find method!", object.Get());

            return DelegateHandler();
        }

        return Delegate<ReturnType, Args...>::Bind([methodName = methodName, object = std::move(object)]<class... ArgTypes>(ArgTypes&&... args) mutable -> ReturnType
            {
                return object->InvokeMethodByName<ReturnType>(methodName, std::forward<ArgTypes>(args)...);
            });
    }

    /*! \brief Call operator overload - alias method for Broadcast().
     *  \tparam ArgTypes The argument types to pass to the handlers.
     *  \param args The arguments to pass to the handlers.
     *  \return The result returned from the final handler that was called, or a default constructed \ref{ReturnType} if no handlers were bound. */
    template <class... ArgTypes>
    HYP_FORCE_INLINE ReturnType operator()(ArgTypes&&... args) const
    {
        return const_cast<ScriptableDelegate*>(this)->Broadcast(std::forward<ArgTypes>(args)...);
    }
};

} // namespace functional
} // namespace hyperion

#endif