/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/String.hpp>
#include <core/containers/HashSet.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Span.hpp>
#include <core/utilities/Variant.hpp>

#include <core/Defines.hpp>
#include <core/Util.hpp>

namespace hyperion {

enum class HypClassAttributeType : uint32
{
    NONE = 0,
    STRING,
    INT,
    FLOAT,
    BOOLEAN
};

struct HypClassAttributeValue
{
    static const HypClassAttributeValue empty;

    HypClassAttributeValue()
        : type(HypClassAttributeType::NONE)
    {
    }

    HypClassAttributeValue(const String& value)
        : type(HypClassAttributeType::STRING),
          value(value)
    {
    }

    HypClassAttributeValue(const char* str)
        : type(HypClassAttributeType::STRING),
          value(String(str))
    {
    }

    HypClassAttributeValue(int value)
        : type(HypClassAttributeType::INT),
          value(value)
    {
    }

    HypClassAttributeValue(double value)
        : type(HypClassAttributeType::FLOAT),
          value(value)
    {
    }

    HypClassAttributeValue(bool value)
        : type(HypClassAttributeType::BOOLEAN),
          value(value)
    {
    }

    HypClassAttributeValue(const HypClassAttributeValue& other) = default;
    HypClassAttributeValue& operator=(const HypClassAttributeValue& other) = default;
    HypClassAttributeValue(HypClassAttributeValue&& other) noexcept = default;
    HypClassAttributeValue& operator=(HypClassAttributeValue&& other) noexcept = default;

    ~HypClassAttributeValue() = default;

    HYP_FORCE_INLINE HypClassAttributeType GetType() const
    {
        return type;
    }

    HYP_FORCE_INLINE bool IsValid() const
    {
        return value.HasValue();
    }

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return GetBool();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !bool(*this);
    }

    HYP_FORCE_INLINE bool operator==(const HypClassAttributeValue& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE bool operator!=(const HypClassAttributeValue& other) const
    {
        return value != other.value;
    }

    HYP_API bool IsString() const;
    HYP_API const String& GetString() const;
    HYP_API bool IsBool() const;
    HYP_API bool GetBool() const;
    HYP_API bool IsInt() const;
    HYP_API int GetInt() const;

    HYP_API String ToString() const;

    // private:
    template <class T>
    HYP_FORCE_INLINE bool Compare(const T& other) const
    {
        bool result = false;

        Visit(value, [&other, &result](auto&& v)
            {
                if constexpr (std::is_same_v<NormalizedType<decltype(v)>, String>)
                {
                    result = false;
                }
                else
                {
                    result = (v == other);
                }
            });

        return result;
    }
    
    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(type);
        hc.Add(value);
        
        return hc;
    }

    HypClassAttributeType type;
    Variant<String, int, double, bool> value;
};

struct HypClassAttribute
{
    HypClassAttribute() = default;

    HypClassAttribute(Name name, const HypClassAttributeValue& value)
        : name(name),
          value(value)
    {
    }
    
    HypClassAttribute(ANSIStringView name, const HypClassAttributeValue& value)
        : name(CreateNameFromDynamicString(name)),
          value(value)
    {
    }

    HypClassAttribute(const HypClassAttribute& other) = default;
    HypClassAttribute& operator=(const HypClassAttribute& other) = default;

    HypClassAttribute(HypClassAttribute&& other) noexcept = default;
    HypClassAttribute& operator=(HypClassAttribute&& other) noexcept = default;

    ~HypClassAttribute() = default;

    HYP_FORCE_INLINE Name GetName() const
    {
        return name;
    }

    HYP_FORCE_INLINE const HypClassAttributeValue& GetValue() const
    {
        return value;
    }

    HYP_FORCE_INLINE bool operator==(const HypClassAttribute& other) const
    {
        return name == other.name && value == other.value;
    }

    HYP_FORCE_INLINE bool operator!=(const HypClassAttribute& other) const
    {
        return name != other.name || value != other.value;
    }
    
    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(name);
        hc.Add(value);
        
        return hc;
    }

    Name name;
    HypClassAttributeValue value;
};

class HypClassAttributeSet
{
public:
    using Iterator = typename HashSet<HypClassAttribute, &HypClassAttribute::name>::Iterator;
    using ConstIterator = typename HashSet<HypClassAttribute, &HypClassAttribute::name>::ConstIterator;

    HypClassAttributeSet() = default;

    HypClassAttributeSet(Span<const HypClassAttribute> attributes)
    {
        if (!attributes)
        {
            return;
        }

        for (const HypClassAttribute& attribute : attributes)
        {
            m_attributes.Insert(attribute);
        }
    }

    HypClassAttributeSet(const HashSet<HypClassAttribute, &HypClassAttribute::name>& attributes)
        : m_attributes(attributes)
    {
    }

    HypClassAttributeSet(HashSet<HypClassAttribute, &HypClassAttribute::name>&& attributes)
        : m_attributes(std::move(attributes))
    {
    }

    HypClassAttributeSet(const HypClassAttributeSet& other) = default;
    HypClassAttributeSet& operator=(const HypClassAttributeSet& other) = default;
    HypClassAttributeSet(HypClassAttributeSet&& other) noexcept = default;
    HypClassAttributeSet& operator=(HypClassAttributeSet&& other) noexcept = default;
    ~HypClassAttributeSet() = default;

    HYP_FORCE_INLINE bool Any() const
    {
        return m_attributes.Any();
    }

    HYP_FORCE_INLINE bool Empty() const
    {
        return m_attributes.Empty();
    }

    HYP_FORCE_INLINE SizeType Size() const
    {
        return m_attributes.Size();
    }

    HYP_FORCE_INLINE const HypClassAttributeValue& operator[](WeakName name) const
    {
        return Get(name);
    }

    const HypClassAttributeValue& Get(WeakName name) const
    {
        static const HypClassAttributeValue invalidValue {};

        return Get(name, invalidValue);
    }

    const HypClassAttributeValue& Get(WeakName name, const HypClassAttributeValue& defaultValue) const
    {
        const auto it = m_attributes.FindAs(name);

        if (it == m_attributes.End())
        {
            return defaultValue;
        }

        return it->GetValue();
    }

    HYP_FORCE_INLINE void Merge(const HypClassAttributeSet& other)
    {
        m_attributes.Merge(other.m_attributes);
    }

    HYP_FORCE_INLINE void Merge(HypClassAttributeSet&& other)
    {
        m_attributes.Merge(std::move(other.m_attributes));
    }

    HYP_FORCE_INLINE Iterator Find(WeakName name)
    {
        return m_attributes.FindAs(name);
    }

    HYP_FORCE_INLINE ConstIterator Find(WeakName name) const
    {
        return m_attributes.FindAs(name);
    }

    HYP_DEF_STL_BEGIN_END(m_attributes.Begin(), m_attributes.End())

private:
    HashSet<HypClassAttribute, &HypClassAttribute::name> m_attributes;
};

} // namespace hyperion
