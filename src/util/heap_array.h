#ifndef HYPERION_UTIL_HEAP_ARRAY_H
#define HYPERION_UTIL_HEAP_ARRAY_H

#include <cstring>

namespace hyperion {

template <class T, size_t ArraySize>
class HeapArray {
public:
    using iterator = T *;

    HeapArray() : ptr(new T[ArraySize]) {}

    HeapArray(const HeapArray &other)
        : ptr(new T[ArraySize])
    {
        for (size_t i = 0; i < ArraySize; i++) {
            ptr[i] = other.ptr[i];
        }
    }

    inline HeapArray &operator=(const HeapArray &other)
    {
        if (this == &other) {
            return *this;
        }

        for (size_t i = 0; i < ArraySize; i++) {
            ptr[i] = other.ptr[i];
        }

        return *this;
    }

    HeapArray(HeapArray &&other) noexcept
        : ptr(nullptr)
    {
        std::swap(other.ptr, ptr);
    }

    inline HeapArray &operator=(const HeapArray &&other) noexcept
    {
        std::swap(other.ptr, ptr);

        return *this;
    }

    ~HeapArray()
    {
        if (ptr != nullptr) {
            delete[] ptr;
        }
    }

    inline T &operator[](size_t index) { return ptr[index]; }
    inline const T &operator[](size_t index) const { return ptr[index]; }

    inline T *Data() { return ptr; }
    inline const T *Data() const { return ptr; }

    inline size_t Size() const { return ArraySize; }

    inline size_t ByteSize() const { return ArraySize * sizeof(T); }

    iterator begin() const { return ptr; }
    iterator end() const { return &ptr[ArraySize - 1]; }

private:
    T *ptr;
};

} // namespace hyperion

#endif