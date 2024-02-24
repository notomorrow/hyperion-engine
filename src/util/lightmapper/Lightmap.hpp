#ifndef HYPERION_V2_LIGHTMAP_HPP
#define HYPERION_V2_LIGHTMAP_HPP

#include <core/lib/DynArray.hpp>
#include <util/img/Bitmap.hpp>

namespace hyperion::v2 {

struct Lightmap
{
    Name        name;
    Bitmap<3>   bitmap;
};

} // namespace hyperion::v2

#endif