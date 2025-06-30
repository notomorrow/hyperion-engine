/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_CORE_HYP_CLASS_REGISTRY_HPP
#define HYPERION_CORE_HYP_CLASS_REGISTRY_HPP

#include <core/utilities/TypeId.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/ForEach.hpp>

#include <core/memory/RefCountedPtr.hpp>

#include <core/containers/TypeMap.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/DataRaceDetector.hpp>

#include <core/Defines.hpp>
#include <core/Name.hpp>
#include <core/Util.hpp>

#include <type_traits>

namespace hyperion {

namespace dotnet {
class Class;
class Object;
} // namespace dotnet

class HypClass;
class HypClassAttribute;
class HypEnum;
struct HypMember;

template <class T>
struct HypClassDefinition;

template <class THypClassDefinition>
class HypClassInstance;

template <class THypClassDefinition>
class HypStructInstance;

template <class THypClassDefinition>
class HypEnumInstance;

template <class T>
struct Handle;

class HYP_API HypClassRegistry
{
public:
    static HypClassRegistry& GetInstance();

    HypClassRegistry();
    HypClassRegistry(const HypClassRegistry& other) = delete;
    HypClassRegistry& operator=(const HypClassRegistry& other) = delete;
    HypClassRegistry(HypClassRegistry&& other) noexcept = delete;
    HypClassRegistry& operator=(HypClassRegistry&& other) noexcept = delete;
    ~HypClassRegistry();

    /*! \brief Get the HypClass instance for the given type.
     *
     *  \tparam T The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered.
     */
    template <class T>
    HYP_FORCE_INLINE const HypClass* GetClass() const
    {
        static_assert(std::is_class_v<T> || std::is_enum_v<T>, "T must be an class or enum type to use GetClass<T>()");

        static constexpr TypeId typeId = TypeId::ForType<NormalizedType<T>>();

        return GetClass(typeId);
    }

    /*! \brief Get the HypClass instance for the given type.
     *
     *  \param typeId The type Id to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered.
     */
    const HypClass* GetClass(TypeId typeId) const;

    /*! \brief Get the HypClass instance associated with the given name.
     *
     *  \param typeName The name of the type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered.
     */
    const HypClass* GetClass(WeakName typeName) const;

    /*! \brief Get the HypClass instance for the given type casted to HypEnum.
     *
     *  \tparam T The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    template <class T>
    HYP_FORCE_INLINE const HypEnumInstance<T>* GetEnum() const
    {
        static_assert(std::is_enum_v<T>, "T must be an enum type to use GetEnum<T>()");

        static constexpr TypeId typeId = TypeId::ForType<NormalizedType<T>>();

        return static_cast<const HypEnumInstance<T>*>(GetEnum(typeId));
    }

    /*! \brief Get the HypClass instance for the given type casted to HypEnum.
     *
     *  \param typeId The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    const HypEnum* GetEnum(TypeId typeId) const;

    /*! \brief Get the HypClass instance for the given type casted to HypEnum.
     *
     *  \param typeId The type to get the HypClass instance for.
     *  \return The HypClass instance for the given type, or nullptr if the type is not registered or is not an enum type
     */
    const HypEnum* GetEnum(WeakName typeName) const;

    void RegisterClass(TypeId typeId, HypClass* hypClass);

    // Only for Dynamic classes
    void UnregisterClass(const HypClass* hypClass);

    void ForEachClass(const ProcRef<IterationResult(const HypClass*)>& callback, bool includeDynamicClasses = true) const;

    void Initialize();

private:
    TypeMap<HypClass*> m_registeredClasses;

    mutable Mutex m_dynamicClassesMutex;
    TypeMap<HypClass*> m_dynamicClasses;

    bool m_isInitialized;

    HashMap<HypClass*, RC<dotnet::Class>> m_managedClasses;
    HashMap<dotnet::Class*, HypClass*> m_managedClassesReverseMapping;
    mutable Mutex m_managedClassesMutex;
};

} // namespace hyperion

#endif