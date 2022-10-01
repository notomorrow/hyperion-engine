#ifndef HYPERION_V2_LIB_C_MEMORY_HPP
#define HYPERION_V2_LIB_C_MEMORY_HPP

#include <Types.hpp>

#include <type_traits>
#include <cstring>

namespace hyperion
{

class Memory
{
public:
    static Int Compare(const void *lhs, const void *rhs, SizeType size)
    {
        return std::memcmp(lhs, rhs, size);
    }

    static char *CopyString(char *dest, const char *src, SizeType length = 0)
    {
        if (length)
            return strncpy(dest, src, length);
        return std::strcpy(dest, src);
    }

    static inline SizeType StringLength(const char *str)
    {
        if (!str) {
            return 0;
        }

        return std::strlen(str);
    }

    static inline void *Set(void *dest, int ch, SizeType length)
    {
        return std::memset(dest, ch, length);
    }

    static inline void *Copy(void *dest, const void *src, SizeType size) 
    {
        return std::memcpy(dest, src, size);
    }

    static void Garble(void *dest, SizeType length)
    {
        std::memset(dest, 0xEFEFEFE, length);
    }

    template <class T, class ...Args>
    static void Construct(void *where, Args &&... args)
    {
        new (where) T(std::forward<Args>(args)...);
    }

    template <class T, class ...Args>
    [[nodiscard]] static void *AllocateAndConstruct(Args &&... args)
    {
        void *ptr = std::malloc(sizeof(T));
        new (ptr) T(std::forward<Args>(args)...);
        return ptr;
    }

    template <class T>
    static void Destruct(T &object)
    {
        object.~T();

#if HYP_DEBUG_MODE
        Memory::Garble(&object, sizeof(T));
#endif
    }

    template <class T>
    static void Destruct(void *ptr)
    {
        static_cast<T *>(ptr)->~T();

#if HYP_DEBUG_MODE
        Memory::Garble(ptr, sizeof(T));
#endif
    }

    template <class T>
    static typename std::enable_if_t<!std::is_same_v<void *, std::add_pointer_t<T>>, void>
    DestructAndFree(void *ptr)
    {
        static_cast<T *>(ptr)->~T();

#if HYP_DEBUG_MODE
        Memory::Garble(ptr, sizeof(T));
#endif

        std::free(ptr);
    }

    template <class T>
    static typename std::enable_if_t<std::is_same_v<void *, std::add_pointer_t<T>>, void>
    DestructAndFree(void *ptr)
    {
        std::free(ptr);
    }
};

}



#endif