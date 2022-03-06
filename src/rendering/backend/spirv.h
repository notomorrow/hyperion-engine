#ifndef BACKEND_SPIRV_H
#define BACKEND_SPIRV_H

#include "../../types.h"
#include "../../hash_code.h"

#include <vector>

namespace hyperion {

struct SPIRVObject {
    using Raw_t = std::vector<ubyte>;

    enum class Type : int {
        UNSET = 0,
        Vertex,
        Fragment,
        Geometry,
        Compute,
        /* Mesh shaders */
        Task,
        Mesh,
        /* Tesselation */
        TessControl,
        TessEval,
        /* Raytracing */
        RayGen,
        RayIntersect,
        RayAnyHit,
        RayClosestHit,
        RayMiss,
    };

    Raw_t raw;
    Type type;

    SPIRVObject() : type(Type::UNSET) {}
    explicit SPIRVObject(Type type) : type(type) {}
    explicit SPIRVObject(Type type, const Raw_t &raw) : type(type), raw(raw) {}
    SPIRVObject(const SPIRVObject &other) : type(other.type), raw(other.raw) {}
    ~SPIRVObject() = default;

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

} // namespace hyperion

#endif