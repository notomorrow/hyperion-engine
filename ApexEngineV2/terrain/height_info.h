#ifndef HEIGHT_INFO_H
#define HEIGHT_INFO_H

#include "../math/vector2.h"
#include "../math/vector3.h"

#include <array>

namespace apex {
enum PageState {
    PageState_unloaded,
    PageState_unloading,
    PageState_loaded
};

struct NeighborChunkInfo {
    Vector2 position;
    bool in_queue = false;

    NeighborChunkInfo()
    {
    }

    NeighborChunkInfo(const Vector2 &position)
        : position(position)
    {
    }

    NeighborChunkInfo(const NeighborChunkInfo &other)
        : position(other.position), in_queue(other.in_queue)
    {
    }
};

struct HeightInfo {
    int width = 64;
    int length = 64;
    int height = 7;
    Vector2 position;
    Vector3 scale;
    PageState pagestate = PageState_unloaded;
    int unload_time = 0;
    std::array<NeighborChunkInfo, 8> neighboring_chunks;

    HeightInfo()
        : position(0), scale(1)
    {
    }

    HeightInfo(const Vector2 &position, const Vector3 &scale)
        : position(position), scale(scale)
    {
    }

    HeightInfo(const HeightInfo &other)
        : width(other.width), length(other.length),
        position(other.position), scale(other.scale), 
        pagestate(other.pagestate), neighboring_chunks(other.neighboring_chunks)
    {
    }
};
}

#endif