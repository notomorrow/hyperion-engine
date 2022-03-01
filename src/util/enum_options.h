#ifndef ENUM_OPTIONS_H
#define ENUM_OPTIONS_H

#include "../hash_code.h"
#include "../math/math_util.h"
#include "../util.h"

#include <array>
#include <initializer_list>

namespace hyperion {

template <typename EnumType, typename ValueType, size_t Sz>
class EnumOptions {
public:
    using EnumOption_t = EnumType;
    using Ordinal_t = uint64_t;
    using EnumValueArray_t = std::array<ValueType, Sz>;
    using EnumValuePair_t = std::pair<EnumType, ValueType>;

    // convert from attachment (2^x) into ordinal (0-5) for use as an array index
    static inline constexpr Ordinal_t EnumToOrdinal(EnumOption_t option)
        { return FastLog2(option); }

    // convert from ordinal (0-5) into power-of-two for use as bit flags
    static inline constexpr EnumOption_t OrdinalToEnum(Ordinal_t ordinal)
        { return EnumOption_t(1 << ordinal); }

    EnumOptions()
        : m_flags(0)
    {
        static_assert(Sz != 0, "EnumOptions cannot have size of zero");
    }

    EnumOptions(const EnumValueArray_t &array)
        : m_values(array),
        m_flags(0)
    {
        UpdateFlags();
    }

    EnumOptions(const std::vector<EnumValuePair_t> &pairs)
        : m_flags(0)
    {
        for (const auto &item : pairs) {
            Set(item.first, item.second);
        }
    }

    EnumOptions(const EnumOptions &other)
        : m_values(other.m_values),
        m_flags(other.m_flags)
    {
        static_assert(Sz != 0, "EnumOptions cannot have size of zero");
    }

    EnumOptions &operator=(const EnumOptions &other)
    {
        m_values = other.m_values;
        m_flags = other.m_flags;

        return *this;
    }

    ~EnumOptions()
    {
    }

    using EnumValuePair_t = std::pair<EnumType, ValueType>;
        { return m_flags & OrdinalToEnum(index); }

    inline constexpr EnumValuePair_t KeyValueAt(size_t index) const
        { return OrdinalToEnum(index); }

    inline constexpr ValueType &ValueAt(size_t index)
        { return m_values[index]; }

    inline constexpr const ValueType &ValueAt(size_t index) const
        { return m_values[index]; }

    inline constexpr EnumValuePair_t KeyValueAt(size_t index) const
        { return std::make_pair(OrdinalToEnum(index), m_values[index]); }

    inline constexpr ValueType &Get(EnumOption_t enum_key)
        { return m_values[EnumToOrdinal(enum_key)]; }

    inline constexpr const ValueType &Get(EnumOption_t enum_key) const
        { return m_values[EnumToOrdinal(enum_key)]; }

    inline EnumOptions &Set(EnumOption_t enum_key, const ValueType &value)
    {
        m_values[EnumToOrdinal(enum_key)] = value;

        AssertThrow(ord < m_values.size());

        m_values[ord] = value;
        m_flags |= uint64_t(enum_key);

        return *this;
    }

    inline EnumOptions &Unset(EnumOption_t enum_key)
    {
        auto ord = EnumToOrdinal(enum_key);

        ex_assert(ord < m_values.size());

        m_values[ord] = ValueType();
        m_flags &= ~uint64_t(enum_key);

        return *this;
    }

    inline bool Has(EnumOption_t enum_key) const
        { return m_flags & uint64_t(enum_key); }

    inline constexpr size_t Size() const
        { return m_values.size(); }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(m_flags);

        for (const auto &it : m_values) {
            hc.Add(it.GetHashCode());
        }

        return hc;
    }

private:
    EnumValueArray_t m_values;
    uint64_t m_flags;

    inline void UpdateFlags()
    {
        m_flags = 0;

        for (int i = 0; i < m_values.size(); i++) {
            m_flags |= OrdinalToEnum(i);
        }
    }
};

} // namespace hyperion

#endif