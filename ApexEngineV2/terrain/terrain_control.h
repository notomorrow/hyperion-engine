#ifndef TERRAIN_CONTROL_H
#define TERRAIN_CONTROL_H

#include "../control.h"
#include "../math/vector2.h"
#include "../math/vector3.h"
#include "terrain_chunk.h"

#include <queue>

#define TERRAIN_MAX_QUEUE_TICK 10
#define TERRAIN_MAX_UPDATE_TICK 4
#define TERRAIN_MAX_UNLOAD_TICK 1
#define TERRAIN_UPDATE_STEP 1

namespace apex {
class TerrainControl : public EntityControl {
public:
    TerrainControl(Camera *camera);
    virtual ~TerrainControl();

    virtual void OnAdded();
    virtual void OnRemoved();
    virtual void OnUpdate(double dt);

protected:
    Camera *m_camera;
    Vector3 m_scale;
    int m_chunk_size = 64;
    std::queue<NeighborChunkInfo*> m_queue;
    std::vector<TerrainChunk*> m_chunks;

    virtual TerrainChunk *NewChunk(const ChunkInfo &chunk_info) = 0;

private:
    void AddChunk(int x, int z);
    TerrainChunk *GetChunk(int x, int z);
    std::array<NeighborChunkInfo, 8> GetNeighbors(int x, int z);

    int m_tick;
    int m_queuetick;
    int m_chunk_index = 0;
    double m_max_distance;
};
} // namespace apex

#endif