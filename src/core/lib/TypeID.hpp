#ifndef HYPERION_V2_LIB_TYPE_ID_HPP
#define HYPERION_V2_LIB_TYPE_ID_HPP

#include <Types.hpp>
#include <HashCode.hpp>

#include <atomic>

namespace hyperion {

struct TypeID {
    using Value = UInt;

private:
    static inline std::atomic<Value> type_id_counter;

public:
    template <class T>
    static TypeID ForType()
    {
        static const TypeID id = TypeID { ++type_id_counter };

        return id;
    }

    Value value;

    TypeID() : value{} {}
    TypeID(const Value &id) : value(id) {}
    TypeID(const TypeID &other) = default;
    TypeID &operator=(const TypeID &other) = default;

    TypeID(TypeID &&other) noexcept
        : value(other.value)
    {
        other.value = 0;
    }
    
    TypeID &operator=(TypeID &&other) noexcept
    {
        value = other.value;
        other.value = 0;

        return *this;
    }

    TypeID &operator=(Value id)
    {
        value = id;

        return *this;
    }

    bool operator==(const TypeID &other) const { return value == other.value; }
    bool operator!=(const TypeID &other) const { return value != other.value; }
    bool operator<(const TypeID &other) const  { return value < other.value; }
    bool operator>(const TypeID &other) const  { return value > other.value; }

    HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(value);

        return hc;
    }
};

} // namespace hyperion

#endif
