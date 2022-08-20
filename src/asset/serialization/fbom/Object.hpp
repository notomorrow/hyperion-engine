#ifndef HYPERION_V2_FBOM_OBJECT_HPP
#define HYPERION_V2_FBOM_OBJECT_HPP

#include <core/lib/Any.hpp>
#include "BaseTypes.hpp"
#include "Loadable.hpp"
#include "Data.hpp"
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

    /*! \brief Drops ownership of the object from the {Any} held inside */
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

class FBOMObject {
public:
    FBOMType m_object_type;
    std::vector<FBOMObject> nodes;
    std::map<std::string, FBOMData> properties;
    FBOMDeserializedObject deserialized;

    FBOMObject()
        : m_object_type(FBOMUnset())
    {
    }

    FBOMObject(const FBOMType &loader_type)
        : m_object_type(loader_type)
    {
    }

    FBOMObject(const FBOMObject &other)
        : m_object_type(other.m_object_type),
          nodes(other.nodes),
          properties(other.properties),
          deserialized(other.deserialized)
    {
    }

    FBOMObject &operator=(const FBOMObject &other)
    {
        m_object_type = other.m_object_type;
        nodes = other.nodes;
        properties = other.properties;
        deserialized = other.deserialized;

        return *this;
    }

    FBOMObject(FBOMObject &&other) noexcept
        : m_object_type(std::move(other.m_object_type)),
          nodes(std::move(other.nodes)),
          properties(std::move(other.properties)),
          deserialized(std::move(other.deserialized))
    {
    }

    FBOMObject &operator=(FBOMObject &&other) noexcept
    {
        m_object_type = std::move(other.m_object_type);
        nodes = std::move(other.nodes);
        properties = std::move(other.properties);
        deserialized = std::move(other.deserialized);

        return *this;
    }

    const FBOMData &GetProperty(const std::string &key) const
    {
        auto it = properties.find(key);

        if (it == properties.end()) {
            return FBOMData::UNSET;
        }

        return it->second;
    }

    inline void SetProperty(const std::string &key, const FBOMData &data)
    {
        properties[key] = data;
    }

    inline void SetProperty(const std::string &key, const FBOMType &type, size_t size, const void *bytes)
    {
        FBOMData data(type);
        data.SetBytes(size, reinterpret_cast<const unsigned char *>(bytes));

        SetProperty(key, data);
    }

    inline void SetProperty(const std::string &key, const FBOMType &type, const void *bytes)
    {
        AssertThrowMsg(!type.IsUnbouned(), "Cannot determine size of an unbounded type, please manually specify size");

        SetProperty(key, type, type.size, bytes);
    }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_object_type.GetHashCode());

        for (const auto &it : nodes) {
            hc.Add(it.GetHashCode());
        }

        for (const auto &it : properties) {
            hc.Add(it.first);
            hc.Add(it.second.GetHashCode());
        }

        return hc;
    }

    inline std::string ToString() const
    {
        std::stringstream ss;

        ss << m_object_type.ToString();
        ss << " { properties: { ";
        for (auto &prop : properties) {
            ss << prop.first;// << ": ";
            //ss << prop.second.ToString();
            ss << ", ";
        }
        ss << " }, nodes: [ ";

        ss << nodes.size();
    
        ss << " ] ";

        ss << " } ";

        return ss.str();
    }
};

} // namespace hyperion::v2::fbom

#endif
