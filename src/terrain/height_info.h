#ifndef HEIGHT_INFO_H
#define HEIGHT_INFO_H

#include "../math/vector2.h"
#include "../math/vector3.h"

#include <array>

#define TERRAIN_CHUNK_WIDTH 256
#define TERRAIN_CHUNK_LENGTH 256
#define TERRAIN_CHUNK_HEIGHT 32

namespace hyperion {

enum class PageState {
    WAITING,
    UNLOADED,
    UNLOADING,
    LOADED
};

struct NeighborChunkInfo {
    Vector2 m_position;
    bool m_in_queue;

    NeighborChunkInfo()
        : m_position(Vector2::Zero()),
          m_in_queue(false)
    {
    }

    NeighborChunkInfo(const Vector2 &position)
        : m_position(position),
          m_in_queue(false)
    {
    }

    NeighborChunkInfo(const NeighborChunkInfo &other)
        : m_position(other.m_position),
          m_in_queue(other.m_in_queue)
    {
    }

    Vector2 Center() const
    {
      return m_position - Vector2(0.5, 0.5);
    }
};

struct ChunkInfo {
    int m_width = TERRAIN_CHUNK_WIDTH;
    int m_length = TERRAIN_CHUNK_LENGTH;
    int m_height = TERRAIN_CHUNK_HEIGHT;
    Vector2 m_position;
    Vector3 m_scale;
    PageState m_page_state;
    int m_unload_time = 0;
    std::array<NeighborChunkInfo, 8> m_neighboring_chunks;

    ChunkInfo()
        : m_position(Vector2::Zero()),
          m_scale(Vector3::One()),
          m_page_state(PageState::UNLOADED)
    {
    }

    ChunkInfo(const Vector2 &position, const Vector3 &scale)
        : m_position(position),
          m_scale(scale),
          m_page_state(PageState::UNLOADED)
    {
    }

    ChunkInfo(const ChunkInfo &other)
        : m_width(other.m_width),
          m_length(other.m_length),
          m_position(other.m_position),
          m_scale(other.m_scale),
          m_page_state(other.m_page_state),
          m_neighboring_chunks(other.m_neighboring_chunks)
    {
    }

    Vector2 Center() const
    {
      return m_position - Vector2(0.5, 0.5);
    }
};

} // namespace hyperion

#endif
