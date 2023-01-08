#ifndef HYPERION_V2_CORE_NAME_HPP
#define HYPERION_V2_CORE_NAME_HPP

#include <Constants.hpp>
#include <core/Core.hpp>
#include <core/lib/StaticString.hpp>
#include <core/lib/String.hpp>
#include <core/lib/DynArray.hpp>
#include <core/lib/HeapArray.hpp>
#include <core/lib/HashMap.hpp>

#include <mutex>

namespace hyperion::v2 {

struct Name;
class NameRegistry;

struct NameInstance
{
    UInt index;
    ANSIString str;
};

struct Name
{
    static const Name invalid;

    static NameRegistry *GetRegistry();

    UInt64 hash_code = 0;

    constexpr Name() = default;

    constexpr Name(UInt64 hash_code)
        : hash_code(hash_code)
    {
    }

    constexpr Name(const Name &other) = default;
    constexpr Name &operator=(const Name &other) = default;
    constexpr Name(Name &&other) noexcept = default;
    constexpr Name &operator=(Name &&other) noexcept = default;

    constexpr bool IsValid() const
        { return hash_code != 0; }

    constexpr explicit operator bool() const
        { return IsValid(); }
    
    constexpr bool operator==(const Name &other) const
        { return hash_code == other.hash_code; }

    constexpr bool operator!=(const Name &other) const
        { return hash_code != other.hash_code; }

    constexpr bool operator<(const Name &other) const
        { return hash_code < other.hash_code; }

    constexpr bool operator<=(const Name &other) const
        { return hash_code <= other.hash_code; }

    constexpr bool operator>(const Name &other) const
        { return hash_code > other.hash_code; }

    constexpr bool operator>=(const Name &other) const
        { return hash_code >= other.hash_code; }

    const ANSIString &LookupString() const;
};

class NameRegistry
{
public:
    static constexpr SizeType num_name_groups = 256;

    NameRegistry()
    {
    }

    NameRegistry(const NameRegistry &other) = delete;
    NameRegistry &operator=(const NameRegistry &other) = delete;
    NameRegistry(NameRegistry &&other) noexcept = delete;
    NameRegistry &operator=(NameRegistry &&other) noexcept = delete;

    Name RegisterName(UInt64 id, const ANSIString &str)
    {
        Name name(id);

        std::lock_guard guard(m_mutex);

        m_name_map.Set(id, str);

        return name;
    }

    const ANSIString &LookupStringForName(Name name) const
    {
        if (!name.IsValid()) {
            return ANSIString::empty;
        }

        std::lock_guard guard(m_mutex);

        const auto it = m_name_map.Find(name.hash_code);

        if (it == m_name_map.End()) {
            return ANSIString::empty;
        }

        return it->value;
    }

private:
    HashMap<UInt64, ANSIString> m_name_map;
    mutable std::mutex m_mutex;
};

struct NameRegistration
{
    UInt64 id;
    
    template <class S>
    static constexpr UInt64 GenerateID()
    {
        using Seq = typename S::Sequence;
        
        constexpr HashCode hash_code = HashCode::GetHashCode(Seq::Data());
        constexpr UInt64 id = hash_code.Value();// IsLittleEndian() ? hash_code.Value() : SwapEndianness(hash_code.Value());

        return id;
    }

    template <class S>
    static NameRegistration FromSequence()
    {
        using Seq = typename S::Sequence;
        
        static constexpr UInt64 id = GenerateID<S>();

        Name::GetRegistry()->RegisterName(id, Seq::Data());

        return NameRegistration { id };
    }
};

template <auto StaticStringType>
struct StaticName
{
    using Sequence = IntegerSequenceFromString<StaticStringType>;
};

template <class S>
static constexpr inline Name NameFromString()
{
    static const NameRegistration name_registration = NameRegistration::FromSequence<S>();

    return Name(name_registration.id);
}

#define HYP_NAME(name) NameFromString<StaticName<StaticString(HYP_STR(name))>>()

} // namespace hyperion::v2

#endif