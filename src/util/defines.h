#ifndef HYPERION_DEFINES_H
#define HYPERION_DEFINES_H

#define HYP_DEF_STRUCT_COMPARATOR \
    bool operator==(const decltype(*this) &other) const { \
        return !std::memcmp(this, &other, sizeof(*this)); \
    }

#endif