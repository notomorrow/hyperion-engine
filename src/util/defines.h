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

#endif