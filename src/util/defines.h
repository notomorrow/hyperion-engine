#ifndef HYPERION_DEFINES_H
#define HYPERION_DEFINES_H

#define HYP_DEF_STRUCT_COMPARATOR \
    bool operator==(const decltype(*this) &other) const { \
        return !std::memcmp(this, &other, sizeof(*this)); \
    }

#define HYP_DEF_STL_HASH(hyp_class) \
    template<> \
    struct std::hash<hyp_class> { \
        size_t operator()(const hyp_class &obj) const \
        { \
            return obj.GetHashCode().Value(); \
        } \
    }


#define HYP_USE_EXCEPTIONS 0

#if HYP_USE_EXCEPTIONS
#define HYP_THROW(msg) throw ::std::runtime_error(msg)
#else
#define HYP_THROW(msg) std::terminate()
#endif

#if defined(__GNUC__) || defined(__GNUG__) || defined(__clang__)
#define HYP_CLANG_OR_GCC 1
#elif defined(_MSC_VER)
#define HYP_MSVC 1
#else
#error Unknown compiler
#endif

#endif