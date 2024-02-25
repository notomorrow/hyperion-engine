#ifndef HYPERION_V2_LIGHTMAP_UV_BUILDER_HPP
#define HYPERION_V2_LIGHTMAP_UV_BUILDER_HPP

#include <core/lib/String.hpp>
#include <rendering/lightmapper/Lightmap.hpp>
#include <rendering/Mesh.hpp>
#include <math/Transform.hpp>

namespace hyperion::v2 {

class LightmapUVBuilder
{
public:
    struct Result
    {
        enum Status
        {
            RESULT_OK,
            RESULT_ERR
        } status;

        String      message;

        Lightmap    result;

        Result()
            : status(RESULT_OK),
              message("")
        { }

        Result(Status status, String message)
            : status(status),
              message(std::move(message))
        { }

        Result(Status status, Lightmap result)
            : status(status),
              result(std::move(result))
        { }

        explicit operator bool() const
            { return status == RESULT_OK; }
    };

    struct LightmapUVBuilderParams
    {
        struct Element
        {
            Handle<Mesh>    mesh;
            Transform       transform;
        };

        Array<Element>  elements;
    };

    LightmapUVBuilder() = default;
    ~LightmapUVBuilder() = default;
    
    Result Build(const LightmapUVBuilderParams &params);
};

} // namespace hyperion::v2

#endif