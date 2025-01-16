/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DOTNET_OBJECT_HPP
#define HYPERION_DOTNET_OBJECT_HPP

#include <core/memory/UniquePtr.hpp>

#include <core/containers/String.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/object/HypData.hpp>

#include <core/ID.hpp>

#include <dotnet/Helpers.hpp>
#include <dotnet/Method.hpp>
#include <dotnet/interop/ManagedObject.hpp>

#include <type_traits>

namespace hyperion {

enum class ObjectFlags : uint32
{
    NONE            = 0x0,
    WEAK_REFERENCE  = 0x1
};

HYP_MAKE_ENUM_FLAGS(ObjectFlags)

} // namespace hyperion

namespace hyperion::dotnet {

class Class;
class Object;
class Method;
class Property;

/*! \brief A move-only object that represents a managed object in the .NET runtime.
 *  By default, the managed object this Object is associated with will be allowed to be released by the .NET runtime upon this object's destruction.
 *  To allow the managed object to live beyond the lifetime of this object, use the ObjectFlags::WEAK_REFERENCE flag.
 * 
 *  \details To create a new Object, use the Class::NewObject method.
 * */
class HYP_API Object
{
public:
    Object();
    Object(Class *class_ptr, ObjectReference object_reference, EnumFlags<ObjectFlags> object_flags = ObjectFlags::NONE);

    Object(const Object &)                  = delete;
    Object &operator=(const Object &)       = delete;

    Object(Object &&other) noexcept;
    Object &operator=(Object &&other) noexcept;

    // Destructor frees the managed object
    ~Object();

    HYP_FORCE_INLINE Class *GetClass() const
        { return m_class_ptr; }

    HYP_FORCE_INLINE const ObjectReference &GetObjectReference() const
        { return m_object_reference; }

    HYP_FORCE_INLINE bool IsValid() const
        { return m_object_reference.guid.IsValid(); }

    /*! \brief Reset the Object to an invalid state.
     *  This will free the managed object if it is still alive unless the ObjectFlags::WEAK_REFERENCE flag is set.
     * */
    void Reset();

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod(const Method *method_ptr, Args &&... args)
    {
        return InvokeMethod_CheckArgs<ReturnType>(method_ptr, std::forward<Args>(args)...);
    }

    template <class ReturnType, class... Args>
    HYP_FORCE_INLINE ReturnType InvokeMethodByName(UTF8StringView method_name, Args &&... args)
    {
        AssertThrow(IsValid());

        const Method *method_ptr = GetMethod(method_name);
        AssertThrowMsg(method_ptr != nullptr, "Method %s not found", method_name.Data());

        return InvokeMethod_CheckArgs<ReturnType>(method_ptr, std::forward<Args>(args)...);
    }

private:
    const Method *GetMethod(UTF8StringView method_name) const;

    void InvokeMethod_Internal(const Method *method_ptr, HypData *args_hyp_data, HypData *out_return_hyp_data);

    template <class ReturnType, class... Args>
    ReturnType InvokeMethod_CheckArgs(const Method *method_ptr, Args &&... args)
    {
        if constexpr (sizeof...(args) != 0) {
            HypData args_hyp_data[] = { HypData(std::forward<Args>(args))... };

            if constexpr (std::is_void_v<ReturnType>) {
                InvokeMethod_Internal(method_ptr, &args_hyp_data[0], nullptr);
            } else {
                HypData return_hyp_data;
                InvokeMethod_Internal(method_ptr, &args_hyp_data[0], &return_hyp_data);

                return std::move(return_hyp_data.Get<ReturnType>());
            }
        } else {
            if constexpr (std::is_void_v<ReturnType>) {
                InvokeMethod_Internal(method_ptr, nullptr, nullptr);
            } else {
                HypData return_hyp_data;
                InvokeMethod_Internal(method_ptr, nullptr, &return_hyp_data);

                return std::move(return_hyp_data.Get<ReturnType>());
            }
        }
    }

    const Property *GetProperty(UTF8StringView method_name) const;

    Class                   *m_class_ptr;
    ObjectReference         m_object_reference;
    EnumFlags<ObjectFlags>  m_object_flags;
};

} // namespace hyperion::dotnet

#endif