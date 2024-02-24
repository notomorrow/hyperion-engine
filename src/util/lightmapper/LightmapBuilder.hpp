#ifndef HYPERION_V2_LIGHTMAP_BUILDER_HPP
#define HYPERION_V2_LIGHTMAP_BUILDER_HPP

#include <rendering/Mesh.hpp>
#include <math/Transform.hpp>
#include <util/lightmapper/Lightmap.hpp>

namespace hyperion::v2 {

class LightmapBuilder
{
public:
    struct Result
    {
        enum Status
        {
            RESULT_OK,
            RESULT_ERR
        } status;

        const char *message = "";

        Lightmap result;

        Result()
            : status(RESULT_OK)
        { }

        Result(Status status, const char *message)
            : status(status),
              message(message)
        { }

        Result(Status status, const char *message, Lightmap result)
            : status(status),
              message(message),
              result(std::move(result))
        { }

        operator bool() const
            { return status == RESULT_OK; }
    };

    struct BuildLightmapParams
    {
        struct Element
        {
            Handle<Mesh>    mesh;
            Transform       transform;
        };

        Array<Element>  elements;
    };

    LightmapBuilder() = default;
    ~LightmapBuilder() = default;
    
    Result Build(const BuildLightmapParams &params);
};

} // namespace hyperion::v2

#endif