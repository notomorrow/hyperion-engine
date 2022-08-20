#ifndef HYPERION_V2_LIB_VARIANT_H
#define HYPERION_V2_LIB_VARIANT_H

#include "CMemory.hpp"
#include "TypeID.hpp"
#include <Constants.hpp>

#include <math/MathUtil.hpp>

#include <system/Debug.hpp>
#include <Types.hpp>

namespace hyperion {

template <class ... Ts>
struct VariantHelper;

template <class T, class ... Ts>
struct VariantHelper<T, Ts...> {
    static inline void CopyAssign(TypeID type_id, void *dst, const void *src)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            *static_cast<NormalizedType<T> *>(dst) = *static_cast<const NormalizedType<T> *>(src);
        } else {
            VariantHelper<Ts...>::CopyAssign(type_id, dst, src);
        }
    }

    static inline bool CopyConstruct(TypeID type_id, void *dst, const void *src)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            Memory::Construct<NormalizedType<T>>(dst, *static_cast<const NormalizedType<T> *>(src));

            return true;
        } else {
            return VariantHelper<Ts...>::CopyConstruct(type_id, dst, src);
        }
    }

    static inline void MoveAssign(TypeID type_id, void *dst, void *src)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            *static_cast<NormalizedType<T> *>(dst) = std::move(*static_cast<NormalizedType<T> *>(src));
        } else {
            VariantHelper<Ts...>::MoveAssign(type_id, dst, src);
        }
    }

    static inline bool MoveConstruct(TypeID type_id, void *dst, void *src)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            Memory::Construct<NormalizedType<T>>(dst, std::move(*static_cast<NormalizedType<T> *>(src)));

            return true;
        } else {
            return VariantHelper<Ts...>::MoveConstruct(type_id, dst, src);
        }
    }

    static inline void Destruct(TypeID type_id, void *data)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            Memory::Destruct<NormalizedType<T>>(data);
        } else {
            VariantHelper<Ts...>::Destruct(type_id, data);
        }
    }

    static inline bool Compare(TypeID type_id, void *data, const void *other_data)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            return *static_cast<NormalizedType<T> *>(data) == *static_cast<const NormalizedType<T> *>(other_data);
        } else {
            return VariantHelper<Ts...>::Compare(type_id, data, other_data);
        }
    }

    static inline HashCode GetHashCode(TypeID type_id, const void *data)
    {
        if (type_id == TypeID::ForType<NormalizedType<T>>()) {
            return HashCode::GetHashCode(*static_cast<const NormalizedType<T> *>(data));
        } else {
            return VariantHelper<Ts...>::GetHashCode(type_id, data);
        }
    }
};

template <>
struct VariantHelper<> {
    static inline void CopyAssign(TypeID type_id, void *dst, const void *src) {}
    static inline bool CopyConstruct(TypeID type_id, void *dst, const void *src) { return false; }
    static inline void MoveAssign(TypeID type_id, void *dst, void *src) {}
    static inline bool MoveConstruct(TypeID type_id, void *dst, void *src) { return false; }

    static inline void Destruct(TypeID type_id, void *data) {}

    static inline bool Destruct(TypeID type_id, void *data, const void *other_data) { return false; }

    static inline HashCode GetHashCode(TypeID type_id, const void *data) { return {}; }
};

template <class ... Types>
class Variant {
    using Helper = VariantHelper<Types...>;

    static const inline TypeID invalid_type_id = TypeID::ForType<void>();

public:

    Variant()
        : m_current_type_id(invalid_type_id)
    {
        Memory::Garble(m_storage.GetPointer(), sizeof(Storage));
    }

    Variant(const Variant &other)
        : m_current_type_id(invalid_type_id)
    {
        Helper::CopyConstruct(other.m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer());

        m_current_type_id = other.m_current_type_id;
    }

    Variant &operator=(const Variant &other)
    {
        if (IsValid()) {
            if (other.m_current_type_id == m_current_type_id) {
                Helper::CopyAssign(m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer());
            } else {
                Helper::Destruct(m_current_type_id, m_storage.GetPointer());

                m_current_type_id = invalid_type_id;

                if (other.IsValid()) {
                    Helper::CopyConstruct(other.m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer());

                    m_current_type_id = other.m_current_type_id;
                }
            }
        } else {
            Helper::CopyConstruct(other.m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer());

            m_current_type_id = other.m_current_type_id;
        }

        AssertThrow(m_current_type_id == other.m_current_type_id);

        return *this;
    }

    Variant &operator=(Variant &&other) noexcept
    {
        if (IsValid()) {
            if (other.m_current_type_id == m_current_type_id) {
                Helper::MoveAssign(m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer());
            } else {
                Helper::Destruct(m_current_type_id, m_storage.GetPointer());

                m_current_type_id = invalid_type_id;

                if (other.IsValid()) {
                    Helper::MoveConstruct(other.m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer());

                    m_current_type_id = other.m_current_type_id;
                }
            }
        } else {
            Helper::MoveConstruct(other.m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer());

            m_current_type_id = other.m_current_type_id;
        }

        AssertThrow(m_current_type_id == other.m_current_type_id);

        other.m_current_type_id = invalid_type_id;
        Memory::Garble(other.m_storage.GetPointer(), sizeof(other.m_storage));

        return *this;
    }

    template <class T>
    explicit Variant(const T &value)
        : m_current_type_id(invalid_type_id)
    {
        const auto type_id = TypeID::ForType<NormalizedType<T>>();

        if (Helper::CopyConstruct(type_id, m_storage.GetPointer(), &value)) {
            m_current_type_id = type_id;
        }
    }

    template <class T>
    explicit Variant(T &&value) noexcept
        : m_current_type_id(invalid_type_id)
    {
        const auto type_id = TypeID::ForType<NormalizedType<T>>();

        if (Helper::MoveConstruct(type_id, m_storage.GetPointer(), &value)) {
            m_current_type_id = type_id;
        }
    }

    template <class T>
    Variant &operator=(const T &value)
    {
        Set(value);

        return *this;
    }

    template <class T>
    Variant &operator=(T &&value) noexcept
    {
        Set(std::forward<T>(value));

        return *this;
    }

    ~Variant()
    {
        if (IsValid()) {
            Helper::Destruct(m_current_type_id, m_storage.GetPointer());
        }
    }

    bool operator==(const Variant &other) const
    {
        if (m_current_type_id != other.m_current_type_id) {
            return false;
        }

        if (!IsValid() && !IsValid()) {
            return true;
        }

        return Helper::Compare(m_current_type_id, m_storage.GetPointer(), other.m_storage.GetPointer());
    }

    template <class T>
    HYP_FORCE_INLINE bool Is() const { return m_current_type_id == TypeID::ForType<NormalizedType<T>>(); }
    HYP_FORCE_INLINE bool IsValid() const { return m_current_type_id != invalid_type_id; }

    template <class T, class ReturnType = NormalizedType<T>>
    bool Get(const ReturnType *out_value) const
    {
        AssertThrow(out_value != nullptr);
        
        if (Is<ReturnType>()) {
            *out_value = *static_cast<const ReturnType *>(m_storage.GetPointer());

            return true;
        }

        return false;
    }

    template <class T>
    T &Get()
    {
        AssertThrowMsg(Is<NormalizedType<T>>(), "Held type differs from requested type!");

        return *static_cast<NormalizedType<T> *>(m_storage.GetPointer());
    }

    template <class T>
    const T &Get() const
    {
        AssertThrowMsg(Is<NormalizedType<T>>(), "Held type differs from requested type!");

        return *static_cast<const NormalizedType<T> *>(m_storage.GetPointer());
    }

    template <class T>
    void Set(const T &value)
    {
        if (IsValid()) {
            Helper::Destruct(m_current_type_id, m_storage.GetPointer());
        }

        m_current_type_id = invalid_type_id;

        const auto type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrowMsg(
            Helper::CopyConstruct(type_id, m_storage.GetPointer(), &value),
            "Not a valid type for the Variant"
        );

        m_current_type_id = type_id;
    }

    template <class T>
    void Set(T &&value)
    {
        if (IsValid()) {
            Helper::Destruct(m_current_type_id, m_storage.GetPointer());
        }

        m_current_type_id = invalid_type_id;

        const auto type_id = TypeID::ForType<NormalizedType<T>>();

        AssertThrowMsg(
            Helper::MoveConstruct(type_id, m_storage.GetPointer(), &value),
            "Not a valid type for the Variant"
        );

        m_current_type_id = type_id;
    }

    HashCode GetHashCode() const
    {
        return Helper::GetHashCode(m_current_type_id, m_storage.GetPointer());
    }

private:
    static constexpr SizeType max_size = MathUtil::Max(sizeof(Types)...);
    static constexpr SizeType max_align = MathUtil::Max(alignof(Types)...);

    TypeID m_current_type_id;

    struct alignas(max_align) Storage {
        alignas(max_align) std::byte data_buffer[max_size];

        void *GetPointer() { return static_cast<void *>(&data_buffer[0]); }
        const void *GetPointer() const { return static_cast<const void *>(&data_buffer[0]); }
    } m_storage;
};

} // namespace hyperion

#endif