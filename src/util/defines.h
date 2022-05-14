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


#if defined(__GNUC__) || defined(__GNUG__) || defined(__clang__)
    #define HYP_CLANG_OR_GCC 1
#elif defined(_MSC_VER)
    #define HYP_MSVC 1
#else
    #error Unknown compiler
#endif


#ifndef HYPERION_BUILD_RELEASE
    #define HYP_DEBUG_MODE 1

    #if HYP_DEBUG_MODE
        #define HYP_ENABLE_BREAKPOINTS 1
    #endif
#endif /* HYPERION_BUILD_RELEASE */



#if HYP_CLANG_OR_GCC
    #define HYP_DEBUG_FUNC_SHORT __FUNCTION__
    #define HYP_DEBUG_FUNC       __PRETTY_FUNCTION__
    #define HYP_DEBUG_LINE       (__LINE__)

    #if HYP_ENABLE_BREAKPOINTS
        #define HYP_BREAKPOINT       (__builtin_trap())
    #endif
#elif HYP_MSVC
    #define HYP_DEBUG_FUNC_SHORT (__FUNCTION__)
    #define HYP_DEBUG_FUNC       (__FUNCSIG__)
    #define HYP_DEBUG_LINE       (__LINE__)
    #if HYP_ENABLE_BREAKPOINTS
        #define HYP_BREAKPOINT       (__debugbreak())
    #endif
#else
    #define HYP_DEBUG_FUNC_SHORT ""
    #define HYP_DEBUG_FUNC ""
    #define HYP_DEBUG_LINE (0)
    #if HYP_ENABLE_BREAKPOINTS
        #define HYP_BREAKPOINT       (void(0))
    #endif
#endif

#if !HYP_ENABLE_BREAKPOINTS
    #define HYP_BREAKPOINT           (void(0))
#endif

#define HYP_USE_EXCEPTIONS 1

#if HYP_USE_EXCEPTIONS
    #define HYP_THROW(msg) throw ::std::runtime_error(msg)
#else
    #if HYP_DEBUG_MODE
        #define HYP_THROW(msg) \
            do { \
                HYP_BREAKPOINT; \
                std::terminate(); \
            } while (0)
    #else
        #define HYP_THROW(msg) std::terminate()
    #endif
#endif

#endif // !HYPERION_DEFINES_H