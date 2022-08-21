#ifndef HYPERION_V2_LIB_REF_COUNTED_PTR_HPP
#define HYPERION_V2_LIB_REF_COUNTED_PTR_HPP

#include <Types.hpp>
#include <Constants.hpp>

#include <atomic>

namespace hyperion {

/*! \brief A simple ref counted pointer class.
    Not atomic by default, but using AtomicRefCountedPtr allows it to be. */
template <class T, class CountType = UInt>
struct RefCountedPtr {
    RefCountedPtr()
        : m_ref(nullptr)
    {
    }

    RefCountedPtr(const T &value)
        : m_ref(new Ref {
              .value = new T(value),
              .count = 1u
          })
    {
    }

    RefCountedPtr(T &&value)
        : m_ref(new Ref {
              .value = new T(std::move(value)),
              .count = 1u
          })
    {
    }

    RefCountedPtr(const RefCountedPtr &other)
        : m_ref(other.m_ref)
    {
        if (m_ref) {
            ++m_ref->count;
        }
    }

    RefCountedPtr &operator=(const RefCountedPtr &other)
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

    RefCountedPtr(RefCountedPtr &&other) noexcept
        : m_ref(other.m_ref)
    {
        other.m_ref = nullptr;
    }

    RefCountedPtr &operator=(RefCountedPtr &&other) noexcept
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

    ~RefCountedPtr()
    {
        if (m_ref) {
            if (--m_ref->count == 0u) {
                delete m_ref;
            }

            m_ref = nullptr;
        }
    }

    T *Get()
        { return m_ref ? m_ref->value : nullptr; }

    const T *Get() const
        { return const_cast<const RefCountedPtr *>(this)->Get(); }

    void Set(const T &value)
    {
        if (m_ref) {
            if (--m_ref->count == 0u) {
                delete m_ref;
            }
        }

        m_ref = new Ref {
            .value = new T(value),
            .count = 1u
        };
    }

    void Set(T &&value)
    {
        if (m_ref) {
            if (--m_ref->count == 0u) {
                delete m_ref;
            }
        }

        m_ref = new Ref {
            .value = new T(std::move(value)),
            .count = 1u
        };
    }

    /*! \brief Takes ownership of {ptr}, dropping the reference to the currently held value,
        if any. Note, do not delete the ptr after passing it to Reset(), as it will be deleted
        automatically. */
    void Reset(T *ptr)
    {
        if (m_ref) {
            if (--m_ref->count == 0u) {
                delete m_ref;
            }

            m_ref = nullptr;
        }

        if (ptr) {
            m_ref = new Ref {
                .value = ptr,
                .count = 1u
            };
        }
    }

    /*! \brief Drops the reference to the currently held value, if any.  */
    void Reset()
    {
        if (m_ref) {
            if (--m_ref->count == 0u) {
                delete m_ref;
            }

            m_ref = nullptr;
        }
    }

protected:
    struct Ref {
        T *value;
        CountType count;

        ~Ref()
        {
            delete value;
        }
    } *m_ref;
};

template <class T, class CountType = UInt>
using AtomicRefCountedPtr = RefCountedPtr<T, std::atomic<CountType>>;

} // namespace hyperion

#endif