#ifndef BACKEND_SPIRV_H
#define BACKEND_SPIRV_H

#include "../../types.h"
#include "../../hash_code.h"

#include <vector>

namespace hyperion {
namespace renderer {
struct SpirvObject {
    using Raw_t = ::std::vector<::hyperion::ubyte>;

    enum class Type : int {
        UNSET = 0,
        VERTEX,
        FRAGMENT,
        GEOMETRY,
        COMPUTE,
        /* Mesh shaders */
        TASK,
        MESH,
        /* Tesselation */
        TESS_CONTROL,
        TESS_EVAL,
        /* Raytracing */
        RAY_GEN,
        RAY_INTERSECT,
        RAY_ANY_HIT,
        RAY_CLOSEST_HIT,
        RAY_MISS,
    };

    Raw_t raw;
    Type type;

    SpirvObject() : type(Type::UNSET) {}
    explicit SpirvObject(Type type) : type(type) {}
    explicit SpirvObject(Type type, const Raw_t &raw) : type(type), raw(raw) {}
    SpirvObject(const SpirvObject &other) : type(other.type), raw(other.raw) {}
    ~SpirvObject() = default;

    inline const uint32_t *VkCode() const { return reinterpret_cast<const uint32_t*>(raw.data()); }

    inline HashCode GetHashCode() const
    {
        HashCode hc;

        hc.Add(uint32_t(type));
        
        for (size_t i = 0; i < raw.size(); i++) {
            hc.Add(raw[i]);
        }

        return hc;
    }
};

} // namespace renderer
} // namespace hyperion

#endif