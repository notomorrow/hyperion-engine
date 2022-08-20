#ifndef HYPERION_V2_FBOM_OBJECT_HPP
#define HYPERION_V2_FBOM_OBJECT_HPP

#include <core/lib/Any.hpp>
#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>

#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMLoadable.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <Types.hpp>

#include <vector>
#include <string>
#include <map>

namespace hyperion::v2::fbom {

struct FBOMDeserializedObject {
    FBOMDeserializedObject()
        : m_ref(nullptr)
    {
    }

    FBOMDeserializedObject(Any &&value)
        : m_ref(new Ref {
              .value = std::move(value),
              .count = 1u
          })
    {
    }

    FBOMDeserializedObject(const FBOMDeserializedObject &other)
        : m_ref(other.m_ref)
    {
        if (m_ref) {
            ++m_ref->count;
        }
    }

    FBOMDeserializedObject &operator=(const FBOMDeserializedObject &other)
    {
        if (m_ref) {
            if (--m_ref->count == 0u) {
                delete m_ref;
            }
        }

        m_ref = other.m_ref;

        if (m_ref) {
            ++m_ref->count;
        }

        return *this;
    }

    FBOMDeserializedObject(FBOMDeserializedObject &&other) noexcept
        : m_ref(other.m_ref)
    {
        other.m_ref = nullptr;
    }

    FBOMDeserializedObject &operator=(FBOMDeserializedObject &&other) noexcept
    {
        if (m_ref) {
            if (--m_ref->count == 0u) {
                delete m_ref;
            }
        }

        m_ref = other.m_ref;

        other.m_ref = nullptr;

        return *this;
    }

    ~FBOMDeserializedObject()
    {
        if (m_ref) {
            if (--m_ref->count == 0u) {
                delete m_ref;
            }

            m_ref = nullptr;
        }
    }

    template <class T>
    T &Get()
    {
        AssertThrow(m_ref != nullptr);
        return m_ref->value.Get<T>();
    }

    template <class T>
    const T &Get() const
    {
        AssertThrow(m_ref != nullptr);
        return m_ref->value.Get<T>();
    }

    template <class T, class ...Args>
    void Set(Args &&... args)
    {
        if (m_ref) {
            if (--m_ref->count == 0u) {
                delete m_ref;
            }
        }

        m_ref = new Ref {
            .value = Any::MakeAny<T, Args...>(std::forward<Args>(args)...),
            .count = 1u
        };
    }

    /*! \brief Drops ownership of the object from the {Any} held inside.
        Be sure to call delete on it when no longer needed!
    */
    template <class T>
    T *Release()
    {
        if (!m_ref) {
            return nullptr;
        }

        auto *ptr = m_ref->value.Release<T>();

        if (--m_ref->count == 0u) {
            delete m_ref;
        }

        m_ref = nullptr;

        return ptr;
    }

private:
    struct Ref {
        Any value;
        UInt count;
    } *m_ref;
};

class FBOMNodeHolder;

class FBOMObject {
public:
    FBOMType m_object_type;
    FBOMNodeHolder *nodes;
    FlatMap<String, FBOMData> properties;
    FBOMDeserializedObject deserialized;

    FBOMObject();
    FBOMObject(const FBOMType &loader_type);
    FBOMObject(const FBOMObject &other);
    FBOMObject &operator=(const FBOMObject &other);
    FBOMObject(FBOMObject &&other) noexcept;
    FBOMObject &operator=(FBOMObject &&other) noexcept;
    ~FBOMObject();

    const FBOMData &GetProperty(const String &key) const;
    void SetProperty(const String &key, const FBOMData &data);
    void SetProperty(const String &key, const FBOMType &type, size_t size, const void *bytes);
    void SetProperty(const String &key, const FBOMType &type, const void *bytes);

    HashCode GetHashCode() const;
    std::string ToString() const;
};

// class FBOMNodeHolder {
// public:
//     FBOMNodeHolder
//         : DynArray<FBOMObject>()
//     {
//     }

//     FBOMNodeHolder(const DynArray<FBOMObject> &other)
//         : DynArray<FBOMObject>(other)
//     {
//     }

//     FBOMNodeHolder &FBOMNodeHolder::operator=(const DynArray<FBOMObject> &other)
//     {
//         DynArray<FBOMObject>::operator=(other);

//         return *this;
//     }

//     FBOMNodeHolder(DynArray<FBOMObject> &&other) noexcept
//         : DynArray<FBOMObject>(std::move(other))
//     {
//     }

//     FBOMNodeHolder &FBOMNodeHolder::operator=(DynArray<FBOMObject> &&other) noexcept
//     {
//         DynArray<FBOMObject>::operator=(std::move(other));

//         return *this;
//     }

// private:
//     DynArray<FBOMObject> objects;
// };

class FBOMNodeHolder : public DynArray<FBOMObject> {
public:
    FBOMNodeHolder()
        : DynArray<FBOMObject>()
    {
    }

    FBOMNodeHolder(const DynArray<FBOMObject> &other)
        : DynArray<FBOMObject>(other)
    {
    }

    FBOMNodeHolder &operator=(const DynArray<FBOMObject> &other)
    {
        DynArray<FBOMObject>::operator=(other);

        return *this;
    }

    FBOMNodeHolder(DynArray<FBOMObject> &&other) noexcept
        : DynArray<FBOMObject>(std::move(other))
    {
    }

    FBOMNodeHolder &operator=(DynArray<FBOMObject> &&other) noexcept
    {
        DynArray<FBOMObject>::operator=(std::move(other));

        return *this;
    }

    ~FBOMNodeHolder() = default;

    HYP_DEF_STL_BEGIN_END(
        reinterpret_cast<typename DynArray<FBOMObject>::ValueType *>(&DynArray<FBOMObject>::m_buffer[DynArray<FBOMObject>::m_start_offset]),
        reinterpret_cast<typename DynArray<FBOMObject>::ValueType *>(&DynArray<FBOMObject>::m_buffer[DynArray<FBOMObject>::m_size])
    )
};

} // namespace hyperion::v2::fbom

#endif
