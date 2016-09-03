#include "terrain_control.h"

#include <thread>

namespace apex {
static int num_threads = 0;
TerrainControl::TerrainControl(Camera *camera)
    : camera(camera), scale(3.0, 2.0, 3.0),
    tick(TERRAIN_MAX_UPDATE_TICK), queuetick(0), 
    maxdist(1.0)
{
}

TerrainControl::~TerrainControl()
{
    for (auto &&it : chunks) {
        if (it != nullptr) {
            delete it;
        }
    }
}

void TerrainControl::OnAdded()
{
    chunks.reserve(12);
    AddChunk(0, 0);
}

void TerrainControl::OnRemoved()
{
}

void TerrainControl::OnUpdate(double dt)
{
    Vector3 campos(camera->GetTranslation());
    campos -= parent->GetGlobalTransform().GetTranslation();
    campos *= Vector3::One() / (scale * (chunk_size - 1));
    Vector2 v2cam(campos.x, campos.z);

    if (queuetick >= TERRAIN_MAX_QUEUE_TICK) {
        if (!_queue.empty()) {
            NeighborChunkInfo *info = _queue.front();
            AddChunk(static_cast<int>(info->position.x), static_cast<int>(info->position.y));
            info->in_queue = false;
            _queue.pop();
        }
        queuetick = 0;
    }
    queuetick += TERRAIN_UPDATE_STEP;

    if (tick >= TERRAIN_MAX_UPDATE_TICK) {
        if (chunk_index >= chunks.size()) {
            chunk_index = 0;
        }

        if (!chunks.empty()) {
            TerrainChunk *chunk = chunks[chunk_index];
            if (chunk != nullptr) {
                switch (chunk->height_info.pagestate) {
                case PageState_loaded:
                    if (chunk->height_info.position.Distance(v2cam) >= maxdist) {
                        chunk->height_info.pagestate = PageState_unloading;
                    } else {
                        if (chunk->entity->GetParent() == nullptr) {
                            parent->AddChild(chunk->entity);
                        }
                        for (auto &it : chunk->height_info.neighboring_chunks) {
                            if (!it.in_queue) {
                                if (it.position.Distance(v2cam) < maxdist) {
                                    it.in_queue = true;
                                    _queue.push(&it);
                                }
                            }
                        }
                    }
                    chunk_index++;
                    break;
                case PageState_unloading:
                    chunk->height_info.unload_time += TERRAIN_UPDATE_STEP;
                    if (chunk->height_info.unload_time >= TERRAIN_MAX_UNLOAD_TICK) {
                        chunk->height_info.pagestate = PageState_unloaded;
                    }
                    break;
                case PageState_unloaded:
                    if (chunk->entity != nullptr && chunk->entity->GetParent() == parent) {
                        parent->RemoveChild(chunk->entity);
                    }
                    chunks.erase(chunks.begin() + chunk_index);
                    delete chunk;
                    break;
                }
            }
        }
        tick = 0;
    }
    tick += TERRAIN_UPDATE_STEP;
}

void TerrainControl::AddChunk(int x, int z)
{
    TerrainChunk *chunk = GetChunk(x, z);
    if (chunk == nullptr) {
        auto lambda = [=]()
        {
            num_threads++;
            HeightInfo height_info(Vector2(x, z), scale);
            height_info.length = chunk_size;
            height_info.width = chunk_size;
            height_info.pagestate = PageState_loaded;
            height_info.neighboring_chunks = GetNeighbors(x, z);

            TerrainChunk *chunk = NewChunk(height_info);

            if (chunk == nullptr) {
                throw "chunk was nullptr";
            } else if (chunk->entity == nullptr) {
                throw "chunk->entity was nullptr";
            }

            chunk->entity->SetLocalTranslation(Vector3(x * (chunk_size - 1) * scale.x, 0, 
                z * (chunk_size - 1) * scale.z));
            chunks.push_back(chunk);
            num_threads--;
        };

#ifdef APEX_MULTITHREADING
        // start thread to create terrain chunk
        std::thread th(lambda);
        th.detach();
#else
        lambda();
#endif
    }
}

TerrainChunk *TerrainControl::GetChunk(int x, int z)
{
    for (auto &&chunk : chunks) {
        if (chunk != nullptr) {
            if ((int)chunk->height_info.position.x == x &&
                (int)chunk->height_info.position.y == z) {
                return chunk;
            }
        }
    }
    return nullptr;
}

std::array<NeighborChunkInfo, 8> TerrainControl::GetNeighbors(int x, int z)
{
    std::array<NeighborChunkInfo, 8> neighbors = {
        Vector2(x + 1, z),
        Vector2(x - 1, z),
        Vector2(x, z + 1),
        Vector2(x, z - 1),
        Vector2(x + 1, z - 1),
        Vector2(x - 1, z - 1),
        Vector2(x + 1, z + 1),
        Vector2(x - 1, z + 1),
    };
    return neighbors;
}
}