#ifndef HYPERION_V2_SERIALIZATION_HPP
#define HYPERION_V2_SERIALIZATION_HPP

#include "fbom/FBOM.hpp"

namespace hyperion::v2 {

using namespace fbom;

class MarshalBase {
public:
    virtual ~MarshalBase();
};

// Empty template class, to be specialized
template <class T>
class Marshal : public MarshalBase {
public:
    virtual ~Marshal();
};

} // namespace hyperion::v2

#endif