#ifndef HYPERION_V2_FBOM_DESERIALIZED_OBJECT_HPP
#define HYPERION_V2_FBOM_DESERIALIZED_OBJECT_HPP

#include <core/lib/Any.hpp>
#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/RefCountedPtr.hpp>

#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMLoadable.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <vector>
#include <string>
#include <map>

namespace hyperion::v2::fbom {

struct FBOMDeserializedObject : RefCountedPtr<Any> {
    FBOMDeserializedObject()
        : RefCountedPtr<Any>()
    {
    }

    FBOMDeserializedObject(Any &&value)
        : RefCountedPtr<Any>(std::forward<Any>(value))
    {
    }

    FBOMDeserializedObject(const FBOMDeserializedObject &other)
        : RefCountedPtr<Any>(other)
    {
    }

    FBOMDeserializedObject &operator=(const FBOMDeserializedObject &other)
    {
        RefCountedPtr<Any>::operator=(other);

        return *this;
    }

    FBOMDeserializedObject(FBOMDeserializedObject &&other) noexcept
        : RefCountedPtr<Any>(std::forward<FBOMDeserializedObject>(other))
    {
    }

    FBOMDeserializedObject &operator=(FBOMDeserializedObject &&other) noexcept
    {
        RefCountedPtr<Any>::operator=(std::forward<FBOMDeserializedObject>(other));

        return *this;
    }

    ~FBOMDeserializedObject() = default;

    /*! \brief Extracts the value held inside the Any */
    template <class T>
    auto Get() -> NormalizedType<T>&
    {
        AssertThrow(RefCountedPtr<Any>::m_ref != nullptr);

        return RefCountedPtr<Any>::Get()->Get<NormalizedType<T>>();
    }

    /*! \brief Extracts the value held inside the Any */
    template <class T>
    auto Get() const -> const NormalizedType<T>&
    {
        AssertThrow(RefCountedPtr<Any>::m_ref != nullptr);

        return RefCountedPtr<Any>::Get()->Get<NormalizedType<T>>();
    }

    template <class T, class ...Args>
    void Set(Args &&... args)
    {
        RefCountedPtr<Any>::Set(Any::Construct<NormalizedType<T>, Args...>(std::forward<Args>(args)...));
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically by the Any. */
    template <class T>
    void Reset(T *ptr)
    {
        auto *any = new Any();
        any->Reset(ptr);

        RefCountedPtr<Any>::Reset(any); // take ownership
    }

    /*! \brief Drops ownership of the object from the {Any} held inside.
        Be sure to call delete on it when no longer needed!

        If no value is currently held, no changes to the underlying object will
        occur. Otherwise, the held value is set to nullptr, and nullptr is returned.
    */
    template <class T>
    [[nodiscard]] T *Release()
    {
        auto *any_ptr = RefCountedPtr<Any>::Get();
        
        if (!any_ptr) {
            return nullptr;
        }

        auto *held_ptr = any_ptr->Release<NormalizedType<T>>();

        RefCountedPtr<Any>::Reset();

        return held_ptr;
    }
};

} // namespace hyperion::v2::fbom

#endif
