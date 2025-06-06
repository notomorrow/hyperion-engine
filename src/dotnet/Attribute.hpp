#ifndef HYPERION_DOTNET_ATTRIBUTE_HPP
#define HYPERION_DOTNET_ATTRIBUTE_HPP

#include <core/memory/UniquePtr.hpp>

#include <core/containers/Array.hpp>
#include <core/containers/HashMap.hpp>

#include <core/utilities/StringView.hpp>

#include <core/Defines.hpp>

namespace hyperion::dotnet {

class Object;
class Class;

struct Attribute
{
    UniquePtr<Object> object;
};

class HYP_API AttributeSet
{
public:
    AttributeSet() = default;

    AttributeSet(Array<Attribute>&& values);

    AttributeSet(const AttributeSet& other) = delete;
    AttributeSet& operator=(const AttributeSet& other) = delete;

    AttributeSet(AttributeSet&& other) noexcept
        : m_values(std::move(other.m_values)),
          m_values_by_name(std::move(other.m_values_by_name))
    {
    }

    AttributeSet& operator=(AttributeSet&& other) noexcept
    {
        if (this != &other)
        {
            m_values = std::move(other.m_values);
            m_values_by_name = std::move(other.m_values_by_name);
        }

        return *this;
    }

    ~AttributeSet() = default;

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_values.Size();
    }

    HYP_FORCE_INLINE bool HasAttribute(UTF8StringView name) const
    {
        return GetAttribute(name) != nullptr;
    }

    HYP_FORCE_INLINE Attribute* GetAttribute(UTF8StringView name) const
    {
        const auto it = m_values_by_name.FindAs(name);

        if (it == m_values_by_name.End())
        {
            return nullptr;
        }

        return it->second;
    }

    HYP_FORCE_INLINE Attribute* GetAttributeByHash(HashCode hash_code) const
    {
        const auto it = m_values_by_name.FindByHashCode(hash_code);

        if (it == m_values_by_name.End())
        {
            return nullptr;
        }

        return it->second;
    }

private:
    Array<Attribute> m_values;
    HashMap<String, Attribute*> m_values_by_name;
};

} // namespace hyperion::dotnet

#endif