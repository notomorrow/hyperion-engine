#ifndef HYPERION_V2_FBOM_OBJECT_HPP
#define HYPERION_V2_FBOM_OBJECT_HPP

#include <core/lib/Any.hpp>
#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/FlatMap.hpp>
#include <core/lib/Optional.hpp>

#include <asset/serialization/fbom/FBOMBaseTypes.hpp>
#include <asset/serialization/fbom/FBOMLoadable.hpp>
#include <asset/serialization/fbom/FBOMData.hpp>
#include <asset/serialization/fbom/FBOMMarshaler.hpp>
#include <asset/serialization/fbom/FBOMDeserializedObject.hpp>

#include <Types.hpp>
#include <Constants.hpp>

#include <vector>
#include <string>
#include <map>
#include <type_traits>

namespace hyperion::v2::fbom {

class FBOMNodeHolder;

struct FBOMExternalObjectInfo {
    String key;

    explicit operator bool() const { return key.Any(); }
};

class FBOMObject {
public:
    FBOMType m_object_type;
    FBOMNodeHolder *nodes;
    FlatMap<String, FBOMData> properties;
    FBOMDeserializedObject deserialized;
    Optional<FBOMExternalObjectInfo> m_external_info;

    FBOMObject();
    FBOMObject(const FBOMType &loader_type);
    FBOMObject(const FBOMObject &other);
    FBOMObject &operator=(const FBOMObject &other);
    FBOMObject(FBOMObject &&other) noexcept;
    FBOMObject &operator=(FBOMObject &&other) noexcept;
    ~FBOMObject();

    bool IsExternal() const
        { return m_external_info.Any(); }

    const String &GetExternalObjectKey() const
        { return IsExternal() ? m_external_info.Get().key : String::empty; }

   const FBOMExternalObjectInfo *GetExternalObjectInfo() const
        { return IsExternal() ? &m_external_info.Get() : nullptr; }

    void SetExternalObjectInfo(const FBOMExternalObjectInfo &info)
    {
        if (info) {
            m_external_info = info;
        } else {
            m_external_info.Unset();
        }
    }

    const FBOMType &GetType() const
        { return m_object_type; }

    const FBOMData &GetProperty(const String &key) const;
    void SetProperty(const String &key, const FBOMData &data);
    void SetProperty(const String &key, const FBOMType &type, SizeType size, const void *bytes);
    void SetProperty(const String &key, const FBOMType &type, const void *bytes);

    template <class T>
    typename std::enable_if_t<!std::is_pointer_v<T>>
    SetProperty(const String &key, const FBOMType &type, const T &value)
    {
        SetProperty(key, type, &value);
    }

    template <class T>
    typename std::enable_if_t<!std::is_pointer_v<T>>
    SetProperty(const String &key, const FBOMType &type, T &&value)
    {
        SetProperty(key, type, &value);
    }

    /*! \brief Add a child object to this object node.
        @param object The child object to add
        @param external If true, store the child object in a separate file,
                        To be pulled in when the final object is loaded */
    template <class T, class Marshaler = FBOMMarshaler<NormalizedType<T>>>
    typename std::enable_if_t<!std::is_same_v<FBOMObject, NormalizedType<T>>, FBOMResult>
    AddChild(const T &object, bool external = false)
    {
        static_assert(implementation_exists<Marshaler>,
            "Marshaler class does not exist");

        static_assert(std::is_base_of_v<FBOMMarshalerBase, Marshaler>,
            "Marshaler class must be a derived class of FBOMMarshalBase");

        Marshaler marshal;

        FBOMObject out_object(marshal.GetObjectType());

        auto result = marshal.Serialize(object, out_object);

        if (result.value != FBOMResult::FBOM_ERR) {
            String external_object_key;

            if (external) {
                external_object_key = String::ToString(out_object.GetHashCode().Value()) + ".fbom";
            }

            AddChild(std::move(out_object), external_object_key);
        }

        return result;
    }
    
    void AddChild(const FBOMObject &object, const String &external_object_key = String::empty);
    void AddChild(FBOMObject &&object, const String &external_object_key = String::empty);

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

    // HYP_DEF_STL_BEGIN_END(
    //     reinterpret_cast<typename DynArray<FBOMObject>::ValueType *>(&DynArray<FBOMObject>::m_buffer[DynArray<FBOMObject>::m_start_offset]),
    //     reinterpret_cast<typename DynArray<FBOMObject>::ValueType *>(&DynArray<FBOMObject>::m_buffer[DynArray<FBOMObject>::m_size])
    // )
};

} // namespace hyperion::v2::fbom

#endif
