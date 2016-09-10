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
    Vector2 m_position;
    bool m_in_queue = false;

    NeighborChunkInfo()
    {
    }

    NeighborChunkInfo(const Vector2 &position)
        : m_position(position)
    {
    }

    NeighborChunkInfo(const NeighborChunkInfo &other)
        : m_position(other.m_position), m_in_queue(other.m_in_queue)
    {
    }
};

struct ChunkInfo {
    int m_width = 64;
    int m_length = 64;
    int m_height = 7;
    Vector2 m_position;
    Vector3 m_scale;
    PageState m_page_state = PageState_unloaded;
    int m_unload_time = 0;
    std::array<NeighborChunkInfo, 8> m_neighboring_chunks;

    ChunkInfo()
        : m_position(0), m_scale(1)
    {
    }

    ChunkInfo(const Vector2 &position, const Vector3 &scale)
        : m_position(position), m_scale(scale)
    {
    }

    ChunkInfo(const ChunkInfo &other)
        : m_width(other.m_width), m_length(other.m_length),
        m_position(other.m_position), m_scale(other.m_scale), 
        m_page_state(other.m_page_state), m_neighboring_chunks(other.m_neighboring_chunks)
    {
    }
};
}

#endif