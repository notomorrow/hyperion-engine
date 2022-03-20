#include "renderer_helpers.h"

#include <math/math_util.h>

namespace hyperion {
namespace renderer {
namespace helpers {

size_t MipmapSize(size_t src_size, int lod)
{
    return MathUtil::Max(src_size >> lod, size_t(1));
}

} // namespace renderer
} // namespace helpers
} // namespace hyperion